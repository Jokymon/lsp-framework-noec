#include <string>
#include <string_view>
#include <lsp/json/json.h>
#include <catch2/catch_test_macros.hpp>

namespace json = lsp::json;

TEST_CASE("json parse handles nested objects and arrays", "[json][parse]")
{
	const auto parsed = json::parse(R"({"name":"lsp","enabled":true,"items":[1,2.5,null]})");

	REQUIRE(parsed.has_value());
	REQUIRE(parsed->isObject());

	const auto object = parsed->object();
	REQUIRE(object.has_value());

	const auto name = object->get("name");
	REQUIRE(name.has_value());
	REQUIRE(name->string().has_value());
	CHECK(name->string().value() == "lsp");

	const auto enabled = object->get("enabled");
	REQUIRE(enabled.has_value());
	REQUIRE(enabled->boolean().has_value());
	CHECK(enabled->boolean().value());

	const auto items = object->get("items");
	REQUIRE(items.has_value());
	REQUIRE(items->array().has_value());
	const auto array = items->array().value();
	REQUIRE(array.size() == 3);
	CHECK(array[0].integer().value() == 1);
	CHECK(array[1].decimal().value() == 2.5);
	CHECK(array[2].isNull());
}

TEST_CASE("json parse rejects duplicate object keys", "[json][parse]")
{
	const auto parsed = json::parse(R"({"dup":1,"dup":2})");

	REQUIRE_FALSE(parsed.has_value());
	CHECK(parsed.error().what() == std::string("Duplicate key 'dup'"));
	CHECK(parsed.error().textPos() == 9);
}

TEST_CASE("json parse rejects trailing commas", "[json][parse]")
{
	const auto parsed = json::parse(R"([1,2,])");

	REQUIRE_FALSE(parsed.has_value());
	CHECK(parsed.error().what() == std::string("Trailing ','"));
	CHECK(parsed.error().textPos() == 4);
}

TEST_CASE("json parse reports trailing characters", "[json][parse]")
{
	const auto parsed = json::parse(R"(true false)");

	REQUIRE_FALSE(parsed.has_value());
	CHECK(parsed.error().what() == std::string("Trailing characters in json"));
	CHECK(parsed.error().textPos() == 5);
}

TEST_CASE("json stringify emits compact representation", "[json][stringify]")
{
	json::Object root;
	root["message"] = json::String{"hello\nworld"};
	root["value"] = json::Decimal{12.3400};
	root["items"] = json::Array{json::Integer{1}, json::Boolean{false}, json::Null{}};

	const auto result = json::stringify(json::Value(std::move(root)));

	CHECK(result.starts_with('{'));
	CHECK(result.ends_with('}'));
	CHECK(result.find(R"("message":"hello\nworld")") != std::string::npos);
	CHECK(result.find(R"("value":12.34)") != std::string::npos);
	CHECK(result.find(R"("items":[1,false,null])") != std::string::npos);

	const auto reparsed = json::parse(result);
	REQUIRE(reparsed.has_value());
	REQUIRE(reparsed->isObject());
}

TEST_CASE("json stringify emits formatted representation", "[json][stringify]")
{
	json::Object root;
	json::Object outer;
	outer["inner"] = json::Integer{7};
	root["outer"] = json::Value(std::move(outer));

	const auto result = json::stringify(json::Value(std::move(root)), true);

	CHECK(result.starts_with("{\n\t"));
	CHECK(result.find(R"("outer": {)") != std::string::npos);
	CHECK(result.find(R"("inner": 7)") != std::string::npos);
	CHECK(result.ends_with("\n}"));
}

TEST_CASE("json string literal helpers round-trip escapes and unicode", "[json][string]")
{
	const std::string original = "line\tbreak\n\"quoted\" \\ slash \x01";
	const auto literal = json::toStringLiteral(original);

	CHECK(literal == "\"line\\tbreak\\n\\\"quoted\\\" \\\\ slash \\u0001\"");
	CHECK(json::fromStringLiteral(literal) == original);
	CHECK(json::fromStringLiteral(R"("\u0041\u00DF")") == "A\xC3\x9F");
}
