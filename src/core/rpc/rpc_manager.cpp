#include "binaryrpc/core/rpc/rpc_manager.hpp"
#include <iostream>

namespace binaryrpc {

/* Context-based registration */
void RPCManager::registerRPC(const std::string& method,
                             RpcContextHandler handler,
                             ITransport* transport)
{
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[method] = [handler, transport](
                            const std::vector<uint8_t>& req,
                            std::vector<uint8_t>& /*resp*/,
                            Session& session)
    {
        RpcContext ctx(std::shared_ptr<Session>(&session, [](Session*) {}),
                       session.liveWs(),
                       transport);
        handler(req, ctx);
    };
}

/* Optional legacy-style registration */
void RPCManager::registerRPC(const std::string& method,
                             InternalHandler handler)
{
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[method] = std::move(handler);
}

/* Invocation */
bool RPCManager::call(const std::string& method,
                      const std::vector<uint8_t>& request,
                      std::vector<uint8_t>& response,
                      Session& session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handlers_.find(method);
    if (it != handlers_.end()) {
        try {
            it->second(request, response, session);
        } catch (const std::exception& ex) {
            LOG_WARN("[RPCManager] Handler exception: " + std::string(ex.what()));
        }
        return true;
    }
    LOG_WARN("[RPCManager] Method not found: " + method );
    return false;
}

}
