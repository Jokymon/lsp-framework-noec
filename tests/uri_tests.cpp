#include <string>
#include <lsp/uri.h>
#include <catch2/catch_test_macros.hpp>

using lsp::Uri;

TEST_CASE("uri parses scheme authority path query and fragment", "[uri][parse]")
{
	const auto uri = Uri::parse("HTTPS://example.com/a%20b?q=%2f#frag%2a");

	REQUIRE(uri.isValid());
	CHECK(uri.scheme() == "https");
	CHECK(uri.hasAuthority());
	CHECK(uri.authority() == "example.com");
	CHECK(uri.path() == "/a b");
	CHECK(uri.hasQuery());
	CHECK(uri.query() == "q=%2F");
	CHECK(uri.hasFragment());
	CHECK(uri.fragment() == "frag%2A");
	CHECK(uri.toString() == "https://example.com/a%20b?q=%2F#frag%2A");
}

TEST_CASE("uri rejects missing scheme separator", "[uri][parse]")
{
	const auto uri = Uri::parse("example.com/path");

	CHECK_FALSE(uri.isValid());
	CHECK(uri.toString().empty());
}

TEST_CASE("uri parse decodes path and preserves round trip", "[uri][parse]")
{
	const auto uri = Uri::parse("file:///tmp/some%20file.txt");

	REQUIRE(uri.isValid());
	CHECK(uri.path() == "/tmp/some file.txt");
	CHECK(uri.toString() == "file:///tmp/some%20file.txt");
}

TEST_CASE("uri setters normalize scheme and encoded hex case", "[uri][mutate]")
{
	Uri uri;

	REQUIRE(uri.setScheme("FiLe"));
	REQUIRE(uri.setAuthority("host%2froot"));
	REQUIRE(uri.setPath("/folder name"));
	REQUIRE(uri.setQuery("name=%2fvalue"));
	REQUIRE(uri.setFragment("frag%2a"));

	CHECK(uri.scheme() == "file");
	CHECK(uri.authority() == "host%2Froot");
	CHECK(uri.path() == "/folder name");
	CHECK(uri.query() == "name=%2Fvalue");
	CHECK(uri.fragment() == "frag%2A");
	CHECK(uri.toString() == "file://host%2Froot/folder%20name?name=%2Fvalue#frag%2A");
}

TEST_CASE("uri remove methods clear optional components", "[uri][mutate]")
{
	auto uri = Uri::parse("custom://authority/path?query#fragment");

	REQUIRE(uri.isValid());
	uri.removeAuthority();
	uri.removeQuery();
	uri.removeFragment();

	CHECK_FALSE(uri.hasAuthority());
	CHECK_FALSE(uri.hasQuery());
	CHECK_FALSE(uri.hasFragment());
	CHECK(uri.authority().empty());
	CHECK(uri.query().empty());
	CHECK(uri.fragment().empty());
	CHECK(uri.toString() == "custom:/path");
}

TEST_CASE("uri encode and decode handle reserved and invalid sequences", "[uri][codec]")
{
	CHECK(Uri::encode("a b/c?", "/") == "a%20b/c%3F");
	CHECK(Uri::decode("a%20b%2Fc") == "a b/c");
	CHECK(Uri::decode("%2g").empty());
}

TEST_CASE("uri setters reject invalid scheme and query", "[uri][mutate]")
{
	Uri uri;

	CHECK_FALSE(uri.setScheme("http:"));
	CHECK_FALSE(uri.setQuery("a#b"));
}
