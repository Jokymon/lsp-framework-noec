#include <cassert>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp::jsonrpc{
namespace{

constexpr std::string_view ProtocolVersion{"2.0"};

std::expected<void, ProtocolError> verifyProtocolVersion(const json::Object& json)
{
	if(!json.contains("jsonrpc"))
		return std::unexpected(ProtocolError{"jsonrpc property is missing"});

	const auto jsonrpc = json.get("jsonrpc").value();

	if(!jsonrpc.isString())
		return std::unexpected(ProtocolError{"jsonrpc property expected to be a string"});

	if(jsonrpc.string().value() != ProtocolVersion)
		return std::unexpected(ProtocolError{"Invalid or unsupported jsonrpc version"});

	return {};
}

std::expected<MessageId, ProtocolError> messageIdFromJson(json::Value json)
{
	if(json.isString())
		return std::move(json.string().value());

	if(json.isNumber())
		return static_cast<json::Integer>(json.number().value());

	if(json.isNull())
		return nullptr;

	return std::unexpected(ProtocolError{"Request id type must be string, number or null"});
}

std::expected<Request, ProtocolError> requestFromJson(json::Object json)
{
	if(auto res = verifyProtocolVersion(json); !res.has_value())
		return std::unexpected(res.error());

	Request request;
	request.method = std::move(json.get("method").value().string().value());

	if(json.contains("id"))
	{
		auto id = messageIdFromJson(json.get("id").value());
		if(!id.has_value())
			return std::unexpected(id.error());
		request.id = std::move(id.value());
	}

	if(json.contains("params"))
	{
		auto params = json.get("params").value();

		if(params.isObject())
			request.params = std::move(params.object().value());
		else if(params.isArray())
			request.params = std::move(params.array().value());
		else if(!params.isNull()) // Be lenient and allow null params even though it is not allowed by jsonrpc 2.0
			return std::unexpected(ProtocolError{"Params type must be object or array"});
	}

	return request;
}

std::expected<Response, ProtocolError> responseFromJson(json::Object json)
{
	if(auto res = verifyProtocolVersion(json); !res.has_value())
		return std::unexpected(res.error());

	Response response;

	if(json.contains("id"))
	{
		auto id = messageIdFromJson(json.get("id").value());
		if(!id.has_value())
			return std::unexpected(id.error());
		response.id = std::move(id.value());
	}

	if(json.contains("result"))
		response.result = std::move(json.get("result").value());

	if(json.contains("error"))
	{
		auto  error         = json.get("error").value();
		auto  errorObj      = error.object().value();
		auto& responseError = response.error.emplace();

		if(!errorObj.contains("code"))
			return std::unexpected(ProtocolError{"Response error is missing the error code"});

		const auto errorCode = errorObj.get("code").value();

		if(!errorCode.isNumber())
			return std::unexpected(ProtocolError{"Response error code must be a number"});

		responseError.code = static_cast<json::Integer>(errorCode.number().value());

		if(!errorObj.contains("message"))
			return std::unexpected(ProtocolError{"Response error is missing the error message"});

		auto errorMessage = errorObj.get("message").value();

		if(!errorMessage.isString())
			return std::unexpected(ProtocolError{"Response error message must be a string"});

		responseError.message = std::move(errorMessage.string().value());

		if(errorObj.contains("data"))
			responseError.data = errorObj.get("data").value();
	}

	if((response.result.has_value() && response.error.has_value()) || (!response.result.has_value() && !response.error.has_value()))
		return std::unexpected(ProtocolError{"Response must have either 'result' or 'error"});

	return response;
}

} // namespace

std::expected<Message, ProtocolError> messageFromJson(json::Object&& json)
{
	if(json.contains("method"))
	{
		auto request = requestFromJson(std::move(json));
		if(!request.has_value())
			return std::unexpected(request.error());
		return std::move(request.value());
	}

	auto response = responseFromJson(std::move(json));
	if(!response.has_value())
		return std::unexpected(response.error());
	return std::move(response.value());
}

std::expected<MessageBatch, ProtocolError> messageBatchFromJson(json::Array&& json)
{
	if(json.empty())
		return std::unexpected(ProtocolError{"Message batch must not be empty"});

	auto batch = MessageBatch();
	batch.reserve(json.size());

	for(auto& jsonMessage : json)
	{
		auto message = messageFromJson(std::move(jsonMessage.object().value()));
		if(!message.has_value())
			return std::unexpected(message.error());
		batch.push_back(std::move(message.value()));
	}

	return batch;
}

json::Object messageToJson(Message&& message)
{
	json::Object json;
	json["jsonrpc"] = std::string{ProtocolVersion};

	if(auto* const request = std::get_if<Request>(&message))
	{
		if(request->id.has_value())
			std::visit([&json](auto& v){ json["id"] = std::move(v); }, *request->id);

		json["method"] = std::move(request->method);

		if(request->params.has_value())
			json["params"] = std::move(*request->params);
	}
	else
	{
		auto& response = std::get<Response>(message);
		assert(response.result.has_value() != response.error.has_value());

		std::visit([&json](auto& v){ json["id"] = std::move(v); }, response.id);

		if(response.result.has_value())
			json["result"] = std::move(*response.result);

		if(response.error.has_value())
		{
			auto& responseError = *response.error;
			auto  errorJson     = json::Object();

			errorJson["code"]    = responseError.code;
			errorJson["message"] = std::move(responseError.message);

			if(responseError.data.has_value())
				errorJson["data"] = std::move(*responseError.data);

			json["error"] = std::move(errorJson);
		}
	}

	return json;
}

json::Array messageBatchToJson(MessageBatch&& batch)
{
	auto json = json::Array();
	json.reserve(batch.size());

	for(auto& message : batch)
		json.push_back(messageToJson(std::move(message)));

	return json;
}

Request createRequest(MessageId id, std::string_view method, std::optional<json::Value> params)
{
	Request request;
	request.id = std::move(id);
	request.method = method;
	request.params = std::move(params);
	return request;
}

Request createNotification(std::string_view method, std::optional<json::Value> params)
{
	Request notification;
	notification.method = method;
	notification.params = std::move(params);
	return notification;
}

Response createResponse(MessageId id, json::Value result)
{
	Response response;
	response.id = std::move(id);
	response.result = std::move(result);
	return response;
}

Response createErrorResponse(MessageId id, json::Integer errorCode, json::String message, std::optional<json::Value> data)
{
	Response response;
	response.id = std::move(id);
	auto& error = response.error.emplace();
	error.code = errorCode;
	error.message = std::move(message);
	error.data = std::move(data);
	return response;
}

} // namespace lsp::jsonrpc
