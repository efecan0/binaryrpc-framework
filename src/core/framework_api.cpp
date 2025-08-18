#include "binaryrpc/core/framework_api.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <iostream>

namespace binaryrpc {

    // Define the implementation struct
    struct FrameworkAPI::Impl {
        SessionManager* sm_;
        ITransport* tr_;

        Impl(SessionManager* sm, ITransport* tr) : sm_(sm), tr_(tr) {}
    };

    // FrameworkAPI methods delegating to the implementation
    FrameworkAPI::FrameworkAPI(SessionManager* sm, ITransport* tr)
        : pImpl_(std::make_unique<Impl>(sm, tr)) {}

    FrameworkAPI::~FrameworkAPI() = default;

    bool FrameworkAPI::sendTo(const std::string& sid,
        const std::vector<uint8_t>& data) const
    {
        if (auto s = pImpl_->sm_->getSession(sid)) {
            if (!s->liveWs()) return false;
            pImpl_->tr_->sendToClient(s->liveWs(), data);
            return true;
        }
        return false;
    }

    void FrameworkAPI::sendToSession(std::shared_ptr<Session> session, const std::vector<uint8_t>& data) {
        if (!session) {
            LOG_ERROR("Invalid session");
            return;
        }
        pImpl_->tr_->sendToSession(session, data);
    }

    bool FrameworkAPI::disconnect(const std::string& sid) const
    {
        if (auto s = pImpl_->sm_->getSession(sid)) {
            pImpl_->tr_->disconnectClient(s->liveWs());
            return true;
        }
        return false;
    }

    std::vector<std::string> FrameworkAPI::listSessionIds() const
    {
        return pImpl_->sm_->listSessionIds();
    }

    std::vector<std::shared_ptr<Session>>
        FrameworkAPI::findBy(const std::string& key,
            const std::string& value) const
    {
        std::vector<std::shared_ptr<Session>> out;
        auto sids = pImpl_->sm_->findIndexed(key, value);
        out.reserve(sids.size());
        for (const auto& sid : sids) {
            if (auto s = pImpl_->sm_->getSession(sid)) {
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
        return pImpl_->sm_->setField(sid, key, value, indexed);
    }

    template<typename T>
    std::optional<T> FrameworkAPI::getField(const std::string& sid,
        const std::string& key) const
    {
        return pImpl_->sm_->getField<T>(sid, key);
    }

    // Explicit template instantiations must be defined in the .cpp file
    template bool FrameworkAPI::setField<std::string>(const std::string&, const std::string&, const std::string&, bool);
    template bool FrameworkAPI::setField<bool>(const std::string&, const std::string&, const bool&, bool);
    template bool FrameworkAPI::setField<int>(const std::string&, const std::string&, const int&, bool);
    template bool FrameworkAPI::setField<uint64_t>(const std::string&, const std::string&, const uint64_t&, bool);
    template bool FrameworkAPI::setField<std::vector<std::string>>(const std::string&, const std::string&, const std::vector<std::string>&, bool);

    template std::optional<std::string> FrameworkAPI::getField<std::string>(const std::string&, const std::string&) const;
    template std::optional<bool> FrameworkAPI::getField<bool>(const std::string&, const std::string&) const;
    template std::optional<int> FrameworkAPI::getField<int>(const std::string&, const std::string&) const;
    template std::optional<uint64_t> FrameworkAPI::getField<uint64_t>(const std::string&, const std::string&) const;
    template std::optional<std::vector<std::string>> FrameworkAPI::getField<std::vector<std::string>>(const std::string&, const std::string&) const;

}
