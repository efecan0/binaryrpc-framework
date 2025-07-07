#include "binaryrpc/plugins/room_plugin.hpp"

namespace binaryrpc {

    RoomPlugin::RoomPlugin(SessionManager& sessionManager, ITransport* transport)
        : sessionManager_(sessionManager), transport_(transport) {
        // Mutex'i initialize
        mtx_ = std::make_unique<std::mutex>();
    }

    void RoomPlugin::initialize() {
        // mutex is initialized in the constructor
    }

    void RoomPlugin::join(const std::string& room, const std::string& sid) {
        if (!mtx_) return;
        std::scoped_lock lk(*mtx_);
        rooms_[room].insert(sid);
    }

    void RoomPlugin::leave(const std::string& room, const std::string& sid) {
        if (!mtx_) return;
        std::scoped_lock lk(*mtx_);
        auto it = rooms_.find(room);
        if (it == rooms_.end()) return;
        it->second.erase(sid);
        if (it->second.empty()) rooms_.erase(it);
    }

    void RoomPlugin::leaveAll(const std::string& sid) {
        if (!mtx_) return;
        
        try {
            std::scoped_lock lk(*mtx_);
            for (auto it = rooms_.begin(); it != rooms_.end(); ) {
                it->second.erase(sid);
                if (it->second.empty())
                    it = rooms_.erase(it);
                else
                    ++it;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("leaveAll: " + std::string(e.what()));
        }
        catch (...) {
            LOG_ERROR("leaveAll: Unknown error");
        }
    }

    void RoomPlugin::broadcast(const std::string& room,
        const std::vector<uint8_t>& data)
    {
        if (!mtx_) return;

        std::vector<std::string> members;

        {   /* --- copy-out under lock --- */
            std::scoped_lock lk(*mtx_);
            auto it = rooms_.find(room);
            if (it == rooms_.end()) return;
            members.assign(it->second.begin(), it->second.end());
        }

        std::vector<std::string> expired;

        for (const auto& sid : members) {
            if (auto session = sessionManager_.getSession(sid);
                session && session->getConnection())
            {
                transport_->sendToClient(session->getConnection(), data);
            }
            else {
                expired.push_back(sid);
            }
        }

        /* clean up disconnected members */
        if (!expired.empty() && mtx_) {
            std::scoped_lock lk(*mtx_);
            auto it = rooms_.find(room);
            if (it != rooms_.end()) {
                for (auto& e : expired) it->second.erase(e);
                if (it->second.empty()) rooms_.erase(it);
            }
        }
    }

    std::vector<std::string> RoomPlugin::getRoomMembers(const std::string& room) const {
        if (!mtx_) return {};
        
        std::scoped_lock lk(*mtx_);
        auto it = rooms_.find(room);
        if (it == rooms_.end()) {
            return {};  // return an empty list if the room does not exist
        }
        
        // return the list of users in the room
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }

}
