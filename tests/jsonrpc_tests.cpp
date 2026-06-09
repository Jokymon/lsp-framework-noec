#include <string>
#include <variant>
#include <lsp/jsonrpc/jsonrpc.h>
#include <catch2/catch_test_macros.hpp>

namespace json = lsp::json;
namespace jsonrpc = lsp::jsonrpc;

TEST_CASE("jsonrpc parses a request with object params", "[jsonrpc][parse]")
{
	json::Object input;
	input["jsonrpc"] = json::String{"2.0"};
	input["id"] = json::Integer{7};
	input["method"] = json::String{"workspace/configuration"};
	json::Object params;
	params["scope"] = json::String{"editor"};
	input["params"] = json::Value(std::move(params));

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE(message.has_value());
	REQUIRE(std::holds_alternative<jsonrpc::Request>(*message));

	const auto& request = std::get<jsonrpc::Request>(*message);
	REQUIRE_FALSE(request.isNotification());
	REQUIRE(request.id.has_value());
	REQUIRE(std::holds_alternative<json::Integer>(*request.id));
	CHECK(std::get<json::Integer>(*request.id) == 7);
	CHECK(request.method == "workspace/configuration");
	REQUIRE(request.params.has_value());
	REQUIRE(request.params->isObject());

	const auto paramsObject = request.params->object();
	REQUIRE(paramsObject.has_value());
	const auto scope = paramsObject.value().get("scope");
	REQUIRE(scope.has_value());
	CHECK(scope.value().string().value() == "editor");
}

TEST_CASE("jsonrpc parses notifications with null params leniently", "[jsonrpc][parse]")
{
	json::Object input;
	input["jsonrpc"] = json::String{"2.0"};
	input["method"] = json::String{"initialized"};
	input["params"] = json::Null{};

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE(message.has_value());
	REQUIRE(std::holds_alternative<jsonrpc::Request>(*message));

	const auto& request = std::get<jsonrpc::Request>(*message);
	CHECK(request.isNotification());
	CHECK_FALSE(request.params.has_value());
}

TEST_CASE("jsonrpc rejects invalid params type", "[jsonrpc][parse]")
{
	json::Object input;
	input["jsonrpc"] = json::String{"2.0"};
	input["method"] = json::String{"textDocument/didOpen"};
	input["params"] = json::Boolean{true};

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE_FALSE(message.has_value());
	CHECK(message.error().what() == std::string("Params type must be object or array"));
}

TEST_CASE("jsonrpc rejects invalid protocol version", "[jsonrpc][parse]")
{
	json::Object input;
	input["jsonrpc"] = json::String{"1.0"};
	input["method"] = json::String{"shutdown"};

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE_FALSE(message.has_value());
	CHECK(message.error().what() == std::string("Invalid or unsupported jsonrpc version"));
}

TEST_CASE("jsonrpc parses a successful response", "[jsonrpc][parse]")
{
	json::Object input;
	input["jsonrpc"] = json::String{"2.0"};
	input["id"] = json::String{"abc"};
	input["result"] = json::Integer{42};

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE(message.has_value());
	REQUIRE(std::holds_alternative<jsonrpc::Response>(*message));

	const auto& response = std::get<jsonrpc::Response>(*message);
	REQUIRE(std::holds_alternative<json::String>(response.id));
	CHECK(std::get<json::String>(response.id) == "abc");
	REQUIRE(response.result.has_value());
	CHECK(response.result->integer().value() == 42);
	CHECK_FALSE(response.error.has_value());
}

TEST_CASE("jsonrpc parses an error response with data", "[jsonrpc][parse]")
{
	json::Object error;
	error["code"] = json::Integer{-32602};
	error["message"] = json::String{"Invalid params"};
	error["data"] = json::String{"details"};

	json::Object input;
	input["jsonrpc"] = json::String{"2.0"};
	input["id"] = json::Integer{3};
	input["error"] = json::Value(std::move(error));

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE(message.has_value());
	REQUIRE(std::holds_alternative<jsonrpc::Response>(*message));

	const auto& response = std::get<jsonrpc::Response>(*message);
	REQUIRE(std::holds_alternative<json::Integer>(response.id));
	CHECK(std::get<json::Integer>(response.id) == 3);
	CHECK_FALSE(response.result.has_value());
	REQUIRE(response.error.has_value());
	CHECK(response.error->code == -32602);
	CHECK(response.error->message == "Invalid params");
	REQUIRE(response.error->data.has_value());
	CHECK(response.error->data->string().value() == "details");
}

TEST_CASE("jsonrpc rejects responses with both result and error", "[jsonrpc][parse]")
{
	json::Object error;
	error["code"] = json::Integer{-32603};
	error["message"] = json::String{"Internal error"};

	json::Object input;
	input["jsonrpc"] = json::String{"2.0"};
	input["id"] = json::Integer{1};
	input["result"] = json::Integer{5};
	input["error"] = json::Value(std::move(error));

	const auto message = jsonrpc::messageFromJson(std::move(input));

	REQUIRE_FALSE(message.has_value());
	CHECK(message.error().what() == std::string("Response must have either 'result' or 'error"));
}

TEST_CASE("jsonrpc rejects empty batches", "[jsonrpc][batch]")
{
	const auto batch = jsonrpc::messageBatchFromJson(json::Array{});

	REQUIRE_FALSE(batch.has_value());
	CHECK(batch.error().what() == std::string("Message batch must not be empty"));
}

TEST_CASE("jsonrpc serializes error response data inside the error object", "[jsonrpc][serialize]")
{
	auto response = jsonrpc::createErrorResponse(
		json::Integer{9},
		jsonrpc::Error::InvalidParams,
		json::String{"bad input"},
		json::String{"details"}
	);

	const auto jsonMessage = jsonrpc::messageToJson(jsonrpc::Message(std::move(response)));

	REQUIRE(jsonMessage.contains("error"));
	CHECK_FALSE(jsonMessage.contains("data"));

	const auto errorValue = jsonMessage.get("error");
	REQUIRE(errorValue.has_value());
	REQUIRE(errorValue.value().isObject());
	const auto errorObject = errorValue.value().object();
	REQUIRE(errorObject.has_value());
	CHECK(errorObject.value().get("code").value().integer().value() == jsonrpc::Error::InvalidParams);
	CHECK(errorObject.value().get("message").value().string().value() == "bad input");
	REQUIRE(errorObject.value().get("data").has_value());
	CHECK(errorObject.value().get("data").value().string().value() == "details");
}

TEST_CASE("jsonrpc round-trips message batches", "[jsonrpc][batch][serialize]")
{
	jsonrpc::MessageBatch batch;
	batch.push_back(jsonrpc::createNotification("initialized"));
	batch.push_back(jsonrpc::createResponse(json::Integer{11}, json::Boolean{true}));

	const auto jsonBatch = jsonrpc::messageBatchToJson(std::move(batch));
	const auto reparsed = jsonrpc::messageBatchFromJson(json::Array(jsonBatch));

	REQUIRE(reparsed.has_value());
	REQUIRE(reparsed->size() == 2);
	CHECK(std::holds_alternative<jsonrpc::Request>((*reparsed)[0]));
	CHECK(std::holds_alternative<jsonrpc::Response>((*reparsed)[1]));
}
