#include "binaryrpc/core/rpc/rpc_context.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/session/session.hpp"
#include <iostream>

namespace binaryrpc {

    RpcContext::RpcContext(std::shared_ptr<Session> sess,
        void* conn,
        ITransport* tr)
        : session_(std::move(sess)), connection_(conn), transport_(tr) {}

    Session& RpcContext::session() { return *session_; }
    const Session& RpcContext::session() const { return *session_; }
    std::shared_ptr<Session> RpcContext::sessionPtr() const { return session_; }


    void RpcContext::reply(const std::vector<uint8_t>& data) const {
        if (transport_ && connection_)
            transport_->sendToClient(connection_, data);
    }

    void RpcContext::broadcast(const std::vector<uint8_t>& data) const {
        if (transport_) transport_->send(data);
    }

    void RpcContext::disconnect() const {
        if (transport_ && connection_) transport_->disconnectClient(connection_);
    }

    bool RpcContext::hasRole(const std::string& expected) const {
        try {
            return session().get<std::string>("role") == expected;
        }
        catch (...) { return false; }
    }

}
