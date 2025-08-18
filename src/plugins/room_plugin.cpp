#include "binaryrpc/plugins/room_plugin.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace binaryrpc {

    struct RoomPlugin::Impl {
        SessionManager& sessionManager_;
        ITransport* transport_;
        std::mutex mtx_;
        std::unordered_map<std::string, std::unordered_set<std::string>> rooms_;

        Impl(SessionManager& sm, ITransport* t) : sessionManager_(sm), transport_(t) {}
    };

    RoomPlugin::RoomPlugin(SessionManager& sessionManager, ITransport* transport)
        : pImpl_(std::make_unique<Impl>(sessionManager, transport)) {}

    RoomPlugin::~RoomPlugin() = default;

    void RoomPlugin::initialize() {}

    void RoomPlugin::join(const std::string& room, const std::string& sid) {
        std::scoped_lock lk(pImpl_->mtx_);
        pImpl_->rooms_[room].insert(sid);
    }

    void RoomPlugin::leave(const std::string& room, const std::string& sid) {
        std::scoped_lock lk(pImpl_->mtx_);
        auto it = pImpl_->rooms_.find(room);
        if (it != pImpl_->rooms_.end()) {
            it->second.erase(sid);
            if (it->second.empty()) {
                pImpl_->rooms_.erase(it);
            }
        }
    }

    void RoomPlugin::leaveAll(const std::string& sid) {
        std::scoped_lock lk(pImpl_->mtx_);
        for (auto it = pImpl_->rooms_.begin(); it != pImpl_->rooms_.end(); ) {
            it->second.erase(sid);
            if (it->second.empty()) {
                it = pImpl_->rooms_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void RoomPlugin::broadcast(const std::string& room, const std::vector<uint8_t>& data) {
        std::vector<std::string> members;
        {
            std::scoped_lock lk(pImpl_->mtx_);
            auto it = pImpl_->rooms_.find(room);
            if (it == pImpl_->rooms_.end()) return;
            members.assign(it->second.begin(), it->second.end());
        }

        for (const auto& sid : members) {
            if (auto session = pImpl_->sessionManager_.getSession(sid)) {
                if(auto* ws = session->liveWs()) {
                    pImpl_->transport_->sendToClient(ws, data);
                }
            }
        }
    }

    std::vector<std::string> RoomPlugin::getRoomMembers(const std::string& room) const {
        std::scoped_lock lk(pImpl_->mtx_);
        auto it = pImpl_->rooms_.find(room);
        if (it == pImpl_->rooms_.end()) {
            return {};
        }
        return {it->second.begin(), it->second.end()};
    }
}
