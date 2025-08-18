#include "binaryrpc/core/app.hpp"
#include "binaryrpc/core/types.hpp"
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include "binaryrpc/core/interfaces/iplugin.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "binaryrpc/core/session/session.hpp"

#include "binaryrpc/core/middleware/middleware_chain.hpp"
#include "internal/core/rpc/rpc_manager.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include "binaryrpc/core/util/error_types.hpp"

#include "binaryrpc/core/util/thread_pool.hpp"
#include <iostream>
#include <thread>

namespace binaryrpc {

    // Define the implementation struct
    struct App::Impl {
        MiddlewareChain middlewareChain_;
        RPCManager rpcManager_;
        SessionManager sessionManager_;
        std::unique_ptr<ITransport> transport_;
        std::vector<std::unique_ptr<IPlugin>> plugins_;
        std::shared_ptr<IProtocol> protocol_;
        std::unique_ptr<ThreadPool> thread_pool_;

        Impl() {
            auto thread_count = std::thread::hardware_concurrency();
            if (thread_count == 0) {
                thread_count = 2;
            }
            thread_pool_ = std::make_unique<ThreadPool>(thread_count);
        }

        ~Impl() {
            // ThreadPool destructor automatically calls join()
        }

        void onDataReceived(const std::shared_ptr<IProtocol>& proto,
                            const std::vector<uint8_t>& data,
                            std::shared_ptr<Session> session,
                            void* connection);
    };

    void App::Impl::onDataReceived(const std::shared_ptr<IProtocol>& proto,
                                  const std::vector<uint8_t>& data,
                                  std::shared_ptr<Session> session,
                                  void* connection)
    {
        auto data_copy = data;
        thread_pool_->add([this, proto, data_copy, session, connection]() {
            if (!proto) {
                LOG_ERROR("[App] protocol_ null (race condition)!");
                return;
            }
            auto req = proto->parse(data_copy);
            if (req.methodName.empty()) {
                ErrorObj err{ RpcErr::Parse,"Failed to parse incoming data" };
                transport_->sendToClient(connection, proto->serializeError(err));
                return;
            }
            bool okChain = middlewareChain_.execute(*session, req.methodName, req.payload);
            if (!okChain) {
                ErrorObj err{ RpcErr::Middleware,"Access denied by middleware" };
                transport_->sendToClient(connection, proto->serializeError(err));
                return;
            }
            std::vector<uint8_t> dummy;
            bool found = rpcManager_.call(req.methodName, req.payload, dummy, *session);
            if (!found) {
                ErrorObj err{ RpcErr::NotFound,"RPC method not found: " + req.methodName };
                transport_->sendToClient(connection, proto->serializeError(err));
                return;
            }
            if (!dummy.empty())
                transport_->sendToClient(connection, dummy);
        });
    }

    // App methods delegating to the implementation
    App& App::getInstance() {
        static App instance;
        return instance;
    }

    App::App() : pImpl_(std::make_unique<Impl>()) {}
    App::~App() = default;

    void App::run(uint16_t port) {
        if (!pImpl_->transport_) {
            LOG_ERROR("[App] Transport not set!");
            return;
        }
        for (auto& p : pImpl_->plugins_) p->initialize();
        pImpl_->transport_->start(port);
    }

    void App::stop() {
        if (pImpl_->transport_) pImpl_->transport_->stop();
    }

    void App::setTransport(std::unique_ptr<ITransport> t) {
        pImpl_->transport_ = std::move(t);
        if (!pImpl_->protocol_) {
            setProtocol(std::make_shared<SimpleTextProtocol>());
            LOG_INFO("[App] Default protocol = SimpleText");
        }
        pImpl_->transport_->setSessionRegisterCallback(
            [this]([[maybe_unused]] const std::string& _id, std::shared_ptr<Session> session) {
                pImpl_->sessionManager_.attachSession(std::move(session));
            });
        std::shared_ptr<IProtocol> protoHold = pImpl_->protocol_;
        pImpl_->transport_->setCallback(
            [this, protoHold](const std::vector<uint8_t>& data,
                              std::shared_ptr<Session> session,
                              void* conn)
            {
                pImpl_->onDataReceived(protoHold, data, std::move(session), conn);
            });
        pImpl_->transport_->setDisconnectCallback(
            [](std::shared_ptr<Session> session) {
                // Handle disconnect if needed
            });
    }

    ITransport* App::getTransport() const { return pImpl_->transport_.get(); }
    SessionManager& App::getSessionManager() { return pImpl_->sessionManager_; }
    const SessionManager& App::getSessionManager() const { return pImpl_->sessionManager_; }

    void App::usePlugin(std::unique_ptr<IPlugin> plugin) {
        plugin->initialize();
        pImpl_->plugins_.push_back(std::move(plugin));
    }

    void App::setProtocol(std::shared_ptr<IProtocol> p) {
        pImpl_->protocol_ = std::move(p);
    }
    IProtocol* App::getProtocol() const { return pImpl_->protocol_.get(); }

    void App::use(Middleware mw) {
        pImpl_->middlewareChain_.add(std::move(mw));
    }

    void App::useFor(const std::string& method, Middleware mw) {
        pImpl_->middlewareChain_.addFor(method, std::move(mw));
    }

    void App::useForMulti(const std::vector<std::string>& methods, Middleware mw) {
        for (const auto& m : methods) {
            pImpl_->middlewareChain_.addFor(m, mw);
        }
    }

    void App::registerRPC(const std::string& method, RpcContextHandler h) {
        pImpl_->rpcManager_.registerRPC(method, std::move(h), pImpl_->transport_.get());
    }
}
