#pragma once

#include <expected>
#include <future>
#include <lsp/error.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

using MessageId = jsonrpc::MessageId;

/*
 * The result returned from a request handler callback that does processing asynchronously
 */
template<typename MessageType>
using AsyncRequestResult = std::future<typename MessageType::Result>;

template<typename MessageType>
using RequestResult = std::expected<typename MessageType::Result, RequestError>;

using NotificationResult = std::expected<void, RequestError>;

using AsyncNotificationResult = std::future<void>;

/*
 * The return type of MessageHandler::sendRequest.
 * id can be used to send a cancel notification (if the request supports it).
 * result will contain the result of the request once it is ready.
 * Do not call result.wait() on the same thread that handles incoming messages as that would result in infinte waiting.
 */
template<typename MessageType>
struct FutureResponse{
	using ResultFuture = std::future<typename MessageType::Result>;

	FutureResponse(MessageId _messageId, ResultFuture _result)
		: messageId{std::move(_messageId)},
		  result{std::move(_result)}
	{
	}

	MessageId    messageId;
	ResultFuture result;
};

} // namespace lsp
