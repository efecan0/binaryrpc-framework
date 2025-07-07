#include "binaryrpc/core/app.hpp"
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <iostream>
#include <thread>

namespace binaryrpc {

    App::App() {
        // Default to number of hardware threads, or 2 if not available.
        auto thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 2;
        }
        thread_pool_ = std::make_unique<folly::CPUThreadPoolExecutor>(thread_count);
    }

    App::~App() {
        if (thread_pool_) {
            thread_pool_->join();
        }
    }

    void App::run(uint16_t port) {
        if (!transport_) {
            LOG_ERROR("[App] Transport set edilmemiş!");
            return;
        }

        for (auto& p : plugins_) p->initialize();

        transport_->start(port);
    }

    void App::stop() {
        if (transport_) transport_->stop();
    }

    void App::setTransport(std::unique_ptr<ITransport> t) {
        transport_ = std::move(t);

        if (!protocol_) {
            setProtocol(std::make_shared<SimpleTextProtocol>());
            LOG_INFO("[App] Default protocol = SimpleText");
        }

        transport_->setSessionRegisterCallback(
            [this]([[maybe_unused]] const std::string& _id, std::shared_ptr<Session> session) {
                sessionManager_.attachSession(std::move(session));
            });

        std::shared_ptr<IProtocol> protoHold = protocol_;
        transport_->setCallback(
            [this, protoHold](const std::vector<uint8_t>& data,
                std::shared_ptr<Session> session,
                void* conn)
            {
                this->onDataReceived(protoHold, data, std::move(session), conn);
            });


        transport_->setDisconnectCallback(
            [this](std::shared_ptr<Session> session) {
            });
    }

    ITransport* App::getTransport() const {return transport_.get();}
    SessionManager& App::getSessionManager() {return sessionManager_;}
    const SessionManager& App::getSessionManager() const {return sessionManager_;}

    void App::usePlugin(std::unique_ptr<IPlugin> plugin) {
        plugin->initialize();
        plugins_.push_back(std::move(plugin));
    }

    void App::setProtocol(std::shared_ptr<IProtocol> p) {
        protocol_ = std::move(p);
    }
    IProtocol* App::getProtocol() const { return protocol_.get(); }

    void App::use(Middleware mw) {
        middlewareChain_.add(std::move(mw));
    }

    void App::useFor(const std::string& method, Middleware mw) {
        middlewareChain_.addFor(method, std::move(mw));
    }

    void App::useForMulti(const std::vector<std::string>& methods, Middleware mw) {
        for (const auto& m : methods) {
            middlewareChain_.addFor(m, mw);
        }
    }

    void App::registerRPC(const std::string& method, RpcContextHandler h) {
        rpcManager_.registerRPC(method, std::move(h), transport_.get());
    }

    void App::onDataReceived(const std::shared_ptr<IProtocol>& proto,
        const std::vector<uint8_t>& data,
        std::shared_ptr<Session>          session,
        void* connection)
    {
        // Copy data to ensure its lifetime extends into the thread
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

}
