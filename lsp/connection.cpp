#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <optional>
#include <string_view>
#include <system_error>
#include <lsp/connection.h>
#include <lsp/error.h>
#include <lsp/io/stream.h>
#include <lsp/json/json.h>

#ifndef LSP_MESSAGE_DEBUG_LOG
	#ifdef NDEBUG
		#define LSP_MESSAGE_DEBUG_LOG 0
	#else
		#define LSP_MESSAGE_DEBUG_LOG 1
	#endif
#endif

#if LSP_MESSAGE_DEBUG_LOG
	#ifdef __APPLE__
		#include <os/log.h>
	#elif defined(_WIN32)
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h>
	#endif
#endif

namespace lsp{
namespace{

/*
 * Message logging
 */

#if LSP_MESSAGE_DEBUG_LOG
void debugLogMessageJson([[maybe_unused]] const std::string& messageType, [[maybe_unused]] const lsp::json::Value& json)
{
#ifdef __APPLE__
	os_log_debug(OS_LOG_DEFAULT, "%{public}s", (messageType + ": " + lsp::json::stringify(json, true)).c_str());
#elif defined(_WIN32)
	OutputDebugStringA((messageType + ": " + lsp::json::stringify(json, true) + '\n').c_str());
#elif defined(__linux__) || defined(__HAIKU__)
    fprintf(stderr, "%s\n",  (messageType + ": " + lsp::json::stringify(json, true)).c_str());
#endif
}
#endif

std::string_view trimWhitespace(std::string_view str)
{
	while(!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
		str.remove_prefix(1);

	while(!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
		str.remove_suffix(1);

	return str;
}

bool equalCaseInsensitive(std::string_view lhs, std::string_view rhs)
{
	return std::ranges::equal(lhs, rhs, [](char a, char b)
		{
			return std::tolower(static_cast<unsigned char>(a)) ==
			       std::tolower(static_cast<unsigned char>(b));
		});
}

std::expected<void, ConnectionError> verifyContentType(std::string_view contentType)
{
	if(!contentType.starts_with("application/vscode-jsonrpc"))
		return std::unexpected(ConnectionError{"Protocol: Unsupported or invalid content type: " + std::string(contentType)});

	constexpr std::string_view charsetKey{"charset="};
	if(const auto idx = contentType.find(charsetKey); idx != std::string_view::npos)
	{
		auto charset = contentType.substr(idx + charsetKey.size());
		charset = trimWhitespace(charset.substr(0, charset.find(';')));

		if(charset != "utf-8" && charset != "utf8")
			return std::unexpected(ConnectionError{"Protocol: Unsupported or invalid character encoding: " + std::string{charset}});
	}

	return {};
}

} // namespace

/*
 * Connection::InputReader
 * Wrapper around io::Stream that allows for peeking and reading single chars
 */

class Connection::InputReader{
public:
	InputReader(io::Stream& stream)
		: m_stream{stream}
	{
	}

	char peek()
	{
		if(!m_peek.has_value())
			m_peek = get();

		return m_peek.value();
	}

	char get()
	{
		if(m_peek.has_value())
		{
			const char c = m_peek.value();
			m_peek.reset();
			return c;
		}

		char c = io::Stream::Eof;
		read(&c, 1);
		return c;
	}

	std::expected<void, ConnectionError> read(char* buffer, std::size_t size)
	{
		if(size > 0)
		{
			if(m_peek.has_value())
			{
				*buffer = m_peek.value();
				m_peek.reset();
				++buffer;
				--size;
			}

			auto res = m_stream.read(buffer, size);
			if(!res.has_value())
				return std::unexpected(ConnectionError(res.error().what()));
		}

		return {};
	}

private:
	io::Stream&         m_stream;
	std::optional<char> m_peek;
};

/*
 * Connection
 */

struct Connection::MessageHeader{
	std::size_t contentLength = 0;
	std::string contentType   = "application/vscode-jsonrpc; charset=utf-8";
};

Connection::Connection(io::Stream& stream)
	: m_stream{stream}
{
}

std::expected<Connection::Message, ConnectionError> Connection::readMessage()
{
	auto readLock = std::unique_lock(m_readMutex);
	auto reader   = InputReader(m_stream);

	if(reader.peek() == io::Stream::Eof)
		return std::unexpected(ConnectionError{"Connection lost"});

	auto header = readMessageHeader(reader);
	if(!header.has_value())
		return std::unexpected(header.error());

	std::string content;
	content.resize(header.value().contentLength);
	if(auto res = reader.read(&content[0], header.value().contentLength); !res.has_value())
		return std::unexpected(res.error());

	readLock.unlock();

	// Verify only after reading the entire message so no partially unread message is left in the stream
	if(auto res = verifyContentType(header.value().contentType); !res.has_value())
		return std::unexpected(res.error());

	auto parseResult = json::parse(content);
	if(!parseResult.has_value())
	{
		(void)writeMessage(jsonrpc::createErrorResponse(json::Null(), MessageError::ParseError, parseResult.error().what()));
		return std::unexpected(ConnectionError(parseResult.error().what()));
	}

	auto json = parseResult.value();
#if LSP_MESSAGE_DEBUG_LOG
	debugLogMessageJson("incoming", json);
#endif

	if(json.isObject())
	{
		auto message = jsonrpc::messageFromJson(std::move(json.object().value()));
		if(!message.has_value())
		{
			(void)writeMessage(jsonrpc::createErrorResponse(json::Null(), MessageError::InvalidRequest, message.error().what()));
			return std::unexpected(ConnectionError(message.error().what()));
		}

		return std::move(message.value());
	}

	if(!json.isArray())
	{
		const auto error = ConnectionError("Message must be a json object or array");
		(void)writeMessage(jsonrpc::createErrorResponse(json::Null(), MessageError::InvalidRequest, error.what()));
		return std::unexpected(error);
	}

	auto batch = jsonrpc::messageBatchFromJson(std::move(json.array().value()));
	if(!batch.has_value())
	{
		(void)writeMessage(jsonrpc::createErrorResponse(json::Null(), MessageError::InvalidRequest, batch.error().what()));
		return std::unexpected(ConnectionError(batch.error().what()));
	}

	return std::move(batch.value());
}

std::expected<void, ConnectionError> Connection::writeMessage(Message&& message)
{
	auto json = json::Value();

	if(auto* const msg = std::get_if<jsonrpc::Message>(&message))
		json = jsonrpc::messageToJson(std::move(*msg));
	else
		json = jsonrpc::messageBatchToJson(std::move(std::get<jsonrpc::MessageBatch>(message)));

#if LSP_MESSAGE_DEBUG_LOG
	debugLogMessageJson("outgoing", json);
#endif
	return writeMessageData(json::stringify(json));
}

std::expected<Connection::MessageHeader, ConnectionError> Connection::readMessageHeader(InputReader& reader)
{
	MessageHeader header;

	while(reader.peek() != '\r')
	{
		if(auto res = readNextMessageHeaderField(header, reader); !res.has_value())
			return std::unexpected(res.error());
	}

	if(reader.get() != '\r' || reader.get() != '\n')
		return std::unexpected(ConnectionError("Protocol: Expected header to be terminated by '\\r\\n'"));

	return header;
}

std::expected<void, ConnectionError> Connection::parseHeaderValue(MessageHeader& header, std::string_view line)
{
	const auto separatorIdx = line.find(':');

	if(separatorIdx != std::string_view::npos)
	{
		const auto key   = trimWhitespace(line.substr(0, separatorIdx));
		const auto value = trimWhitespace(line.substr(separatorIdx + 1));

		if(equalCaseInsensitive(key, "Content-Length"))
		{
			const auto* first    = value.data();
			const auto* last     = first + value.size();
			const auto [ptr, ec] = std::from_chars(first, last, header.contentLength);

			if(ec != std::errc{} || ptr != last)
				return std::unexpected(ConnectionError("Protocol: Invalid value for Content-Length header field"));
		}
		else if(equalCaseInsensitive(key, "Content-Type"))
		{
			header.contentType = std::string{value.data(), value.size()};
		}
	}

	return {};
}

std::expected<void, ConnectionError> Connection::readNextMessageHeaderField(MessageHeader& header, InputReader& reader)
{
	if(reader.peek() == std::char_traits<char>::eof())
		return std::unexpected(ConnectionError{"Connection lost"});

	std::string lineData;

	while(reader.peek() != '\r')
	{
		const auto c = reader.get();

		if(c == '\n')
			return std::unexpected(ConnectionError("Protocol: Unexpected '\\n' in header field, expected '\\r\\n'"));

		lineData.push_back(c);
	}

	if(auto res = parseHeaderValue(header, lineData); !res.has_value())
		return std::unexpected(res.error());

	if(reader.get() != '\r' || reader.get() != '\n')
		return std::unexpected(ConnectionError("Protocol: Expected header field to be terminated by '\\r\\n'"));

	return {};
}

std::expected<void, ConnectionError> Connection::writeMessageData(const std::string& content)
{
	std::lock_guard lock{m_writeMutex};
	MessageHeader header{content.size()};
	const auto messageStr = messageHeaderString(header) + content;
	auto res = m_stream.write(messageStr.data(), messageStr.size());
	if(!res.has_value())
		return std::unexpected(ConnectionError(res.error().what()));

	return {};
}

std::string Connection::messageHeaderString(const MessageHeader& header)
{
	return "Content-Length: " + std::to_string(header.contentLength) + "\r\n" +
	       "Content-Type: " + header.contentType + "\r\n\r\n";
}

} // namespace lsp
