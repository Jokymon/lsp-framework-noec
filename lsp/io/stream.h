#pragma once

#include <cstddef>
#include <expected>
#include <lsp/exception.h>

namespace lsp::io{

/*
 * Error
 */

class Error : public lsp::Exception{
public:
	using lsp::Exception::Exception;
};

/*
 * Stream
 */

class Stream{
public:
	Stream& operator=(const Stream&) = delete;

	static constexpr auto Eof = static_cast<char>(0xff);

	virtual ~Stream() = default;
	virtual std::expected<void, Error> read(char* buffer, std::size_t size) = 0;
	virtual std::expected<void, Error> write(const char* buffer, std::size_t size) = 0;

protected:
	Stream() = default;
	Stream(Stream&&) = default;
	Stream& operator=(Stream&&) = default;
};

} // namespace lsp::io
