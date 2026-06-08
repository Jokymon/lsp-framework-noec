#pragma once

#include <expected>
#include <mutex>
#include <string>
#include <variant>
#include <lsp/exception.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{
namespace json{
class Value;
} // namespace json

namespace io{
class Stream;
} // namespace io

/*
 * Error returned when then connection to a client is unexpectedly lost
 */
class ConnectionError : public Exception{
public:
	using Exception::Exception;
};

/*
 * Connection between the server and a client.
 * I/O happens via lsp::io::Stream so the underlying implementation can be anything from stdio to sockets
 */
class Connection{
public:
	using Message = std::variant<jsonrpc::Message, jsonrpc::MessageBatch>;

	Connection(io::Stream& stream);

	std::expected<Message, ConnectionError> readMessage();
	std::expected<void, ConnectionError> writeMessage(Message&& message);

private:
	io::Stream& m_stream;
	std::mutex  m_readMutex;
	std::mutex  m_writeMutex;

	struct MessageHeader;
	class InputReader;

	std::expected<MessageHeader, ConnectionError> readMessageHeader(InputReader& reader);
	static std::expected<void, ConnectionError> parseHeaderValue(MessageHeader& header, std::string_view line);
	static std::expected<void, ConnectionError> readNextMessageHeaderField(MessageHeader& header, InputReader& reader);
	std::expected<void, ConnectionError> writeMessageData(const std::string& content);
	std::string messageHeaderString(const MessageHeader& header);
};

} // namespace lsp
