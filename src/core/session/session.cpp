#include "binaryrpc/core/session/session.hpp"
#include "internal/core/qos/duplicate_filter.hpp"
#include <unordered_map>
#include <string>
#include <any>
#include <vector>
#include <chrono>

namespace binaryrpc {

    // Define the implementation struct
    struct Session::Impl {
        ClientIdentity ident_;
        std::string legacyId_;
        std::unordered_map<std::string, std::any> data_;
        WS* liveWs_ = nullptr;
        void* legacyConn_ = nullptr;
        qos::DuplicateFilter dupFilter_;

        Impl(ClientIdentity ident, std::string legacyId)
            : ident_(std::move(ident)), legacyId_(std::move(legacyId)) {}
    };

    // Session methods delegating to the implementation
    Session::Session(ClientIdentity ident, std::string legacyId)
        : pImpl_(std::make_unique<Impl>(std::move(ident), std::move(legacyId))) {}

    Session::~Session() = default;

    const std::string& Session::id() const {
        return pImpl_->legacyId_;
    }

    const ClientIdentity& Session::identity() const {
        return pImpl_->ident_;
    }

    void Session::rebind(WS* ws) {
        pImpl_->liveWs_ = ws;
        pImpl_->dupFilter_ = qos::DuplicateFilter();
    }

    Session::WS* Session::liveWs() const {
        return pImpl_->liveWs_;
    }

    void Session::setConnection(void* conn) {
        pImpl_->legacyConn_ = conn;
    }

    void* Session::getConnection() const {
        return pImpl_->legacyConn_;
    }

    bool Session::acceptDuplicate(const std::vector<uint8_t>& rpcPayload, std::chrono::milliseconds ttl) {
        return pImpl_->dupFilter_.accept(rpcPayload, ttl);
    }

    void Session::set_any(const std::string& key, std::any value) {
        pImpl_->data_[key] = std::move(value);
    }

    std::any Session::get_any(const std::string& key) const {
        auto it = pImpl_->data_.find(key);
        if (it != pImpl_->data_.end()) {
            return it->second;
        }
        return {};
    }

}