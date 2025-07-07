#include "binaryrpc/core/framework_api.hpp"
#include <iostream>
#include <algorithm>

namespace binaryrpc {

    /* ctor -----------------------------------------------------*/
    FrameworkAPI::FrameworkAPI(SessionManager* sm, ITransport* tr)
        : sm_{ sm }, tr_{ tr } {}

    /* send data to a single user ------------------------------*/
    bool FrameworkAPI::sendTo(const std::string& sid,
        const std::vector<uint8_t>& data) const
    {
        if (auto s = sm_->getSession(sid)) {
            if (!s->liveWs()) return false;
            tr_->sendToClient(s->liveWs(), data);
            return true;
        }
        return false;
    }


    void FrameworkAPI::sendToSession(std::shared_ptr<Session> session, const std::vector<uint8_t>& data) {
        if (!session) {
            LOG_ERROR("Invalid session");
            return;
        }
        tr_->sendToSession(session, data);

    }

    /* disconnect the connection  -----------------------------------------*/
    bool FrameworkAPI::disconnect(const std::string& sid) const
    {
        if (auto s = sm_->getSession(sid)) {
            tr_->disconnectClient(s->liveWs());
            return true;
        }
        return false;
    }

    /* all active session IDs ---------------------------------*/
    std::vector<std::string> FrameworkAPI::listSessionIds() const
    {
        return sm_->listSessionIds();
    }

    std::vector<std::shared_ptr<Session>>
        FrameworkAPI::findBy(const std::string& key,
            const std::string& value) const
    {
        std::vector<std::shared_ptr<Session>> out;
        auto& idx = sm_->indices();
        auto sids = idx.find(key, value);
        out.reserve(sids.size());
        for (const auto& sid : sids) {
            if (auto s = sm_->getSession(sid)) {
                out.push_back(s);
            }
        }
        return out;
    }

    template<typename T>
    bool FrameworkAPI::setField(const std::string& sid,
        const std::string& key,
        const T& value,
        bool indexed)
    {
        return sm_->setField(sid, key, value, indexed);
    }

    template<typename T>
    std::optional<T> FrameworkAPI::getField(const std::string& sid,
        const std::string& key) const
    {
        return sm_->getField<T>(sid, key);
    }

    // Explicit template instantiation
    template bool FrameworkAPI::setField<std::string>(
        const std::string& sid,
        const std::string& key,
        const std::string& value,
        bool indexed);

    template bool FrameworkAPI::setField<bool>(
        const std::string& sid,
        const std::string& key,
        const bool& value,
        bool indexed);

    template bool FrameworkAPI::setField<int>(
        const std::string& sid,
        const std::string& key,
        const int& value,
        bool indexed);

    template bool FrameworkAPI::setField<uint64_t>(
        const std::string& sid,
        const std::string& key,
        const uint64_t& value,
        bool indexed);

    template std::optional<std::string> FrameworkAPI::getField<std::string>(
        const std::string& sid,
        const std::string& key) const;

    template std::optional<bool> FrameworkAPI::getField<bool>(
        const std::string& sid,
        const std::string& key) const;

    template std::optional<int> FrameworkAPI::getField<int>(
        const std::string& sid,
        const std::string& key) const;

    template std::optional<uint64_t> FrameworkAPI::getField<uint64_t>(
        const std::string& sid,
        const std::string& key) const;

    // Vector template instantiations
    template bool FrameworkAPI::setField<std::vector<std::string>>(
        const std::string& sid,
        const std::string& key,
        const std::vector<std::string>& value,
        bool indexed);

    template std::optional<std::vector<std::string>> FrameworkAPI::getField<std::vector<std::string>>(
        const std::string& sid,
        const std::string& key) const;
}
