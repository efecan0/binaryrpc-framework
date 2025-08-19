#include "binaryrpc/core/session/session_manager.hpp"
#include "internal/core/session/generic_index.hpp"
#include "internal/core/util/conn_state.hpp"
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/auth/ClientIdentity.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include "internal/core/util/random.hpp"
#include "internal/core/util/time.hpp"
#include <iomanip>
#include <sstream>

using namespace binaryrpc;

/*──────────────── Helper: Hex string ───────────────*/
static std::string toHex(const std::array<uint8_t, 16>& arr) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : arr) {
        ss << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

/**
 * @brief Move constructor for SessionManager.
 */
SessionManager::SessionManager(SessionManager&& o) noexcept
    : ttlMs_{ o.ttlMs_ },
      index_{ o.index_ },
      cleanupThread_{ std::move(o.cleanupThread_) },
      sessions_{ std::move(o.sessions_) },
      bySid_{ std::move(o.bySid_) },
      byId_{ std::move(o.byId_) },
      state_{ std::move(o.state_) }
{
    // Set the source index to null to prevent double-free, as ownership has been transferred.
    o.index_ = nullptr;
}

/**
 * @brief Move assignment operator for SessionManager.
 */
SessionManager& SessionManager::operator=(SessionManager&& o) noexcept
{
    if (this != &o) {
        // Release existing resources before acquiring new ones.
        if (cleanupThread_.joinable()) {
            cleanupThread_.request_stop();
            cleanupThread_.join();
        }
        delete index_;

        // Transfer ownership of resources from the source object.
        ttlMs_ = o.ttlMs_;
        index_ = o.index_;
        cleanupThread_ = std::move(o.cleanupThread_);
        sessions_ = std::move(o.sessions_);
        bySid_ = std::move(o.bySid_);
        byId_ = std::move(o.byId_);
        state_ = std::move(o.state_);

        // Set the source index to null to prevent double-free.
        o.index_ = nullptr;
    }
    return *this;
}

/*──────────────── Helper: new SID generator ───────────────*/
static std::atomic<std::uint64_t> g_sid{ 1 };
static inline std::string makeSid() { return "S" + std::to_string(g_sid++); }

SessionManager::SessionManager(std::uint64_t ttlMs)
    : ttlMs_{ ttlMs }, index_{ new GenericIndex() } {}

SessionManager::~SessionManager() {
    if (cleanupThread_.joinable()) {
        cleanupThread_.request_stop();
        cleanupThread_.join();
    }
    delete index_;
}

GenericIndex& SessionManager::indices() { return *index_; }

std::unordered_set<std::string>
SessionManager::findIndexed(const std::string& key, const std::string& value) const {
    return index_->find(key, value);
}

/*──────────────── Session creation ───────────────*/
std::shared_ptr<Session> SessionManager::createSession(const ClientIdentity& cid, uint64_t nowMs)
{
    auto sid = makeSid();
    auto sess = std::make_shared<Session>(cid, sid);
    sess->expiryMs = nowMs + ttlMs_;
    sess->qosState = std::make_shared<ConnState>();  // Initialize QoS state
    
    std::scoped_lock wlk(mx_);
    byId_[cid] = sess;
    bySid_[sid] = sess;
    sessions_[sid] = sess;
    
    return sess;
}

/*──────────────── State operations ───────────────*/
template<typename T>
bool SessionManager::setField(const std::string& sid,
    const std::string& key,
    const T& value,
    bool indexed)
{
    std::unique_lock lk(stateMx_);
    
    // 1. Update state
    state_[sid][key] = value;
    
    // 2. Index if required
    if (indexed) {
        index_->add(sid, key, toStr(value));
    }
    
    return true;
}

template<typename T>
std::optional<T> SessionManager::getField(const std::string& sid,
    const std::string& key) const
{
    std::shared_lock lk(stateMx_);
    if (auto it = state_.find(sid); it != state_.end()) {
        if (auto kit = it->second.find(key); kit != it->second.end()) {
            return std::any_cast<T>(kit->second);
        }
    }
    return std::nullopt;
}

/*──────────────── New API: identity-based ─────────*/
std::shared_ptr<Session>
SessionManager::getOrCreate(const ClientIdentity& cid, uint64_t nowMs)
{
    // 1. Check if token exists
    bool hasToken = std::any_of(cid.sessionToken.begin(), cid.sessionToken.end(),
        [](uint8_t b) { return b != 0; });

    LOG_DEBUG(std::format("getOrCreate: client='{}', device='{}', hasToken={}", 
        cid.clientId, cid.deviceId, hasToken));

    // 2. If no token, create a new session
    if (!hasToken) {
        ClientIdentity modCid = cid;
        randomFill(modCid.sessionToken);  // Generate new token
        LOG_DEBUG(std::format("No token, creating new session with token={}", 
            toHex(modCid.sessionToken)));
        return createSession(modCid, nowMs);
    }

    /* 3. Check if session already exists */
    {
        std::shared_lock rlk(mx_);
        auto it = byId_.find(cid);
        if (it != byId_.end()) {
            auto s = it->second;
            
            // Has the session expired?
            if (s->expiryMs && nowMs > s->expiryMs) {
                // Expired → create new session
                LOG_DEBUG(std::format("Session expired, creating new one. Old expiry={}, now={}", 
                    s->expiryMs, nowMs));
                rlk.unlock();
                return createSession(cid, nowMs);
            }

            /* --- token verification ---------------------------------- */
            if (std::memcmp(s->identity().sessionToken.data(),
                cid.sessionToken.data(), 16) == 0)
            {
                LOG_DEBUG(std::format("Found existing session with token={}", 
                    toHex(s->identity().sessionToken)));
                s->expiryMs = nowMs + ttlMs_;
                return s;                    // < SAME session
            }
            /* Token mismatch ⇒ spoofing or new device → drop and create new session */
            LOG_DEBUG(std::format("Token mismatch. Expected={}, got={}", 
                toHex(s->identity().sessionToken), toHex(cid.sessionToken)));
        }
    }

    /* 4. Create a new session */
    LOG_DEBUG(std::format("Creating new session for client='{}' with token={}", 
        cid.clientId, toHex(cid.sessionToken)));
    return createSession(cid, nowMs);
}

/*──────────────── TTL Reaper ───────────────*/
void SessionManager::reap(uint64_t now)
{
    std::scoped_lock lk(mx_);
    for (auto it = byId_.begin(); it != byId_.end(); ) {
        auto& sess = it->second;
        if (!sess->liveWs() && ttlMs_ && sess->expiryMs < now) {
            // 1. Remove associated state
            {
                std::unique_lock stateLk(stateMx_);
                state_.erase(sess->id());
            }

            // 2. Release QoS buffer
            sess->qosState.reset();

            // 3. Remove from all maps and index
            bySid_.erase(sess->id());
            sessions_.erase(sess->id());
            index_->remove(sess->id());

            it = byId_.erase(it);
        }
        else {
            ++it;
        }
    }
}

/*──────────────── Manual attach / detach ───────────*/
void SessionManager::attachSession(std::shared_ptr<Session> s)
{
    std::scoped_lock lk(mx_);
    sessions_[s->id()] = s;
    bySid_[s->id()] = s;
}

void SessionManager::removeSession(const std::string& sid)
{
    std::scoped_lock lk(mx_);

    if (auto it = sessions_.find(sid); it != sessions_.end()) {
        auto sess = it->second;
        byId_.erase(sess->identity());
        bySid_.erase(sid);
        sessions_.erase(it);
    }
    index_->remove(sid);
}

/*──────────────── Get / List operations ───────────────*/
std::shared_ptr<Session> SessionManager::getSession(const std::string& sid) const
{
    std::shared_lock lk(mx_);
    if (auto it = sessions_.find(sid); it != sessions_.end())
        return it->second;
    return nullptr;
}

std::vector<std::string> SessionManager::listSessionIds() const
{
    std::vector<std::string> out;
    std::shared_lock lk(mx_);
    out.reserve(sessions_.size());
    for (auto& [sid, _] : sessions_) out.emplace_back(sid);
    return out;
}

/*──────────────── indexedSet<T> (template) ───*/
template<typename T>
void SessionManager::indexedSet(std::shared_ptr<Session> s,
                                const std::string& key,
                                const T& value)
{
    s->set(key, value);
    index_->add(s->id(), key, toStr(value));
}

/* explicit instantiation */
template void SessionManager::indexedSet<std::string>(
    std::shared_ptr<Session>, const std::string&, const std::string&);
template void SessionManager::indexedSet<int>(
    std::shared_ptr<Session>, const std::string&, const int&);
template void SessionManager::indexedSet<bool>(
    std::shared_ptr<Session>, const std::string&, const bool&);

void SessionManager::startCleanupTimer() {
    cleanupThread_ = std::jthread([this](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            // Check every 1 minute
            std::this_thread::sleep_for(std::chrono::minutes(1));
            
            uint64_t now = clockMs();  // Use clockMs instead of epochMillis
            reap(now);  // Clean up expired sessions
        }
    });
}

// Explicit template instantiation
template bool SessionManager::setField<std::string>(
    const std::string& sid,
    const std::string& key,
    const std::string& value,
    bool indexed);

template bool SessionManager::setField<int>(
    const std::string& sid,
    const std::string& key,
    const int& value,
    bool indexed);

template bool SessionManager::setField<uint64_t>(
    const std::string& sid,
    const std::string& key,
    const uint64_t& value,
    bool indexed);

template bool SessionManager::setField<bool>(
    const std::string& sid,
    const std::string& key,
    const bool& value,
    bool indexed);

template std::optional<std::string> SessionManager::getField<std::string>(
    const std::string& sid,
    const std::string& key) const;

template std::optional<int> SessionManager::getField<int>(
    const std::string& sid,
    const std::string& key) const;

template std::optional<uint64_t> SessionManager::getField<uint64_t>(
    const std::string& sid,
    const std::string& key) const;

template std::optional<bool> SessionManager::getField<bool>(
    const std::string& sid,
    const std::string& key) const;

// Vector template instantiations
template bool SessionManager::setField<std::vector<std::string>>(
    const std::string& sid,
    const std::string& key,
    const std::vector<std::string>& value,
    bool indexed);

template std::optional<std::vector<std::string>> SessionManager::getField<std::vector<std::string>>(
    const std::string& sid,
    const std::string& key) const;

bool SessionManager::addOfflineMessage(const std::string& sessionId, const std::vector<uint8_t>& data) {
    std::scoped_lock lk(queueMutex);
    
    // Clear expired messages
    cleanupOldMessages();
    
    // Global queue limit check
    if (totalQueuedMessages >= MAX_TOTAL_QUEUED_MESSAGES) {
        LOG_WARN("Global message queue limit reached");
        return false;
    }

    // Per-session queue limit check
    auto& queue = offlineQueues[sessionId];
    if (queue.size() >= MAX_QUEUE_SIZE_PER_SESSION) {
        LOG_WARN("Session message queue limit reached for session: " + sessionId);
        return false;
    }

    // Add message
    queue.push({data, clockMs(), sessionId});
    totalQueuedMessages++;
    
    LOG_DEBUG("Added offline message to queue for session: " + sessionId + 
              ", queue size: " + std::to_string(queue.size()));
    return true;
}

void SessionManager::processOfflineMessages(const std::string& sessionId, 
                                         std::function<void(const std::vector<uint8_t>&)> sendCallback) {
    std::scoped_lock lk(queueMutex);
    
    auto it = offlineQueues.find(sessionId);
    if (it == offlineQueues.end()) {
        LOG_DEBUG("No offline messages found for session: " + sessionId);
        return;
    }

    auto& queue = it->second;
    size_t messageCount = queue.size();
    
    LOG_DEBUG("Processing " + std::to_string(messageCount) + 
              " offline messages for session: " + sessionId);

    while (!queue.empty()) {
        auto& msg = queue.front();
        sendCallback(msg.data);
        queue.pop();
        totalQueuedMessages--;
    }
    
    offlineQueues.erase(it);
    LOG_DEBUG("Finished processing offline messages for session: " + sessionId);
}


void SessionManager::cleanupOldMessages() {
    auto now = clockMs();
    size_t cleanedCount = 0;
    
    for (auto it = offlineQueues.begin(); it != offlineQueues.end();) {
        auto& queue = it->second;
        
        // Clean up old messages based on TTL
        while (!queue.empty() && 
               (now - queue.front().timestamp > MESSAGE_TTL_MS)) {
            queue.pop();
            totalQueuedMessages--;
            cleanedCount++;
        }
        
        // Remove empty queues
        if (queue.empty()) {
            it = offlineQueues.erase(it);
        } else {
            ++it;
        }
    }
    
    if (cleanedCount > 0) {
        LOG_DEBUG("Cleaned up " + std::to_string(cleanedCount) + " old messages");
    }
}
