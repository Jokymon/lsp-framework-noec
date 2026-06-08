#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <lsp/io/stream.h>
#include <lsp/io/standardio.h>

namespace lsp::io{
namespace{

class StandardIOStream : public Stream{
public:
	StandardIOStream()
	{
#ifdef _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
		_setmode(_fileno(stdout), _O_BINARY);
#endif
	}

	std::expected<void, Error> read(char* buffer, std::size_t size) override
	{
		if(std::fread(buffer, size, 1, stdin) < 1 && std::ferror(stdin) != 0)
			return std::unexpected(Error(std::strerror(errno)));

		return {};
	}

	std::expected<void, Error> write(const char* buffer, std::size_t size) override
	{
		if(std::fwrite(buffer, size, 1, stdout) < 1)
			return std::unexpected(Error(std::strerror(errno)));

		std::fflush(stdout);
		return {};
	}
};

}

Stream& standardIO()
{
	static auto stream = StandardIOStream();
	return stream;
}

} // namespace lsp::io
