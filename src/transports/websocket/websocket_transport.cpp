/**
 * @file websocket_transport.cpp
 * @brief Implementation of the WebSocketTransport class for BinaryRPC.
 *
 * This file contains the implementation details for the WebSocket transport, including session management,
 * Quality of Service (QoS) logic, message delivery, and connection lifecycle management. It uses uWebSockets
 * for high-performance WebSocket communication and supports advanced features such as offline message queuing,
 * duplicate detection, and pluggable handshake inspection.
 */
#include "binaryrpc/transports/websocket/websocket_transport.hpp"

// Project-specific internal and public headers that were moved from the header
#include "binaryrpc/core/session/session_manager.hpp"
#include "internal/core/util/conn_state.hpp"
#include "binaryrpc/core/util/qos.hpp"
#include "binaryrpc/core/interfaces/IHandshakeInspector.hpp"
#include "binaryrpc/core/strategies/exponential_backoff.hpp"
#include "binaryrpc/core/util/DefaultInspector.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include "binaryrpc/core/session/session.hpp"
#include "internal/core/util/hex.hpp"
#include "internal/core/util/time.hpp"
#include <uwebsockets/App.h>
#include <utility>

#ifdef _WIN32
    #include <intrin.h>
#else
    #include <endian.h>
#endif

// Platform-independent byte order conversion functions
namespace {
    /**
     * @brief Converts a 64-bit integer from host to network byte order (big endian).
     * @param value The value to convert.
     * @return The value in network byte order.
     */
    inline uint64_t hostToNetwork64(uint64_t value) {
#ifdef _WIN32
        return _byteswap_uint64(value);
#else
        return htobe64(value);
#endif
    }

    /**
     * @brief Converts a 64-bit integer from network byte order (big endian) to host byte order.
     * @param value The value to convert.
     * @return The value in host byte order.
     */
    inline uint64_t networkToHost64(uint64_t value) {
#ifdef _WIN32
        return _byteswap_uint64(value);
#else
        return be64toh(value);
#endif
    }
}

#include <ankerl/unordered_dense.h>
#include <shared_mutex>
#include <mutex>
#include <set>

using namespace binaryrpc;

namespace binaryrpc {

    // These definitions are internal to the WebSocketTransport implementation
    // and are moved here from the public header.

    struct PerSocketData {
        std::shared_ptr<Session>   session;
        std::shared_ptr<ConnState> state;
        std::chrono::steady_clock::time_point lastActive;
        std::atomic_bool          alive;
        uWS::Loop*                 loop;
        std::deque<std::vector<uint8_t>> sendQueue;

        PerSocketData();
        PerSocketData(const PerSocketData& o);
        PerSocketData& operator=(const PerSocketData& o);
        PerSocketData(PerSocketData&& other) noexcept;
        PerSocketData& operator=(PerSocketData&& other) noexcept;
    };
    
    // Implementations for PerSocketData's methods
    PerSocketData::PerSocketData(): state{std::make_shared<ConnState>()}, alive{true}, loop{nullptr} {}

    PerSocketData::PerSocketData(const PerSocketData& o)
        : session(o.session), state(o.state), lastActive(o.lastActive), 
          alive(true), loop(nullptr), sendQueue() {}

    PerSocketData& PerSocketData::operator=(const PerSocketData& o) {
        if (this != &o) {
            session = o.session;
            state = o.state;
            lastActive = o.lastActive;
            alive.store(true, std::memory_order_relaxed);
            loop = nullptr;
            sendQueue.clear();
        }
        return *this;
    }

    PerSocketData::PerSocketData(PerSocketData&& other) noexcept
        : session(std::move(other.session)), state(std::move(other.state)), 
          lastActive(other.lastActive), alive(other.alive.load(std::memory_order_relaxed)),
          loop(other.loop), sendQueue(std::move(other.sendQueue))
    {
        other.loop = nullptr;
    }

    PerSocketData& PerSocketData::operator=(PerSocketData&& other) noexcept {
        if (this != &other) {
            session = std::move(other.session);
            state = std::move(other.state);
            lastActive = other.lastActive;
            alive.store(other.alive.load(std::memory_order_relaxed), std::memory_order_relaxed);
            loop = other.loop;
            sendQueue = std::move(other.sendQueue);
            other.loop = nullptr;
        }
        return *this;
    }

    /**
     * @brief Converts a FrameType enum value to its string representation for debugging/logging.
     * @param type The frame type.
     * @return A string representation of the frame type.
     */
    std::string frameTypeToString(FrameType type) {
        switch (type) {
            case FRAME_DATA: return "DATA";
            case FRAME_ACK: return "ACK";
            case FRAME_PREPARE: return "PREPARE";
            case FRAME_PREPARE_ACK: return "PREPARE_ACK";
            case FRAME_COMMIT: return "COMMIT";
            case FRAME_COMPLETE: return "COMPLETE";
            default: return "UNKNOWN(" + std::to_string(static_cast<int>(type)) + ")";
        }
    }

    using WS = uWS::WebSocket<false, true, PerSocketData>;

    /**
     * @class WebSocketTransport::Impl
     * @brief Private implementation class for WebSocketTransport (PIMPL idiom).
     *
     * Encapsulates all internal state, connection management, retry logic, and advanced features
     * such as QoS, offline message queuing, and duplicate detection. Not exposed to users of the public API.
     */
    class WebSocketTransport::Impl {
    public:
        /**
         * @brief Constructs the Impl object with session manager and configuration.
         * @param sm Reference to the SessionManager.
         * @param idle Idle timeout in seconds.
         * @param maxPayload Maximum payload size in bytes.
         */
        Impl(SessionManager& sm, uint16_t idle, uint32_t maxPayload)
            : smgr(sm), idleTimeout(idle), maxPay(maxPayload) {
            // default backoff strategy
            opts.backoffStrategy = std::make_shared<ExponentialBackoff>(
                std::chrono::milliseconds(opts.baseRetryMs),
                std::chrono::milliseconds(opts.maxBackoffMs));
            running = true;
        }

        std::shared_ptr<IHandshakeInspector> inspector = std::make_shared<DefaultInspector>();

        /**
         * @brief Checks if the given WebSocket connection is alive and valid.
         * @param ws Pointer to the WebSocket.
         * @param impl Pointer to the implementation instance.
         * @return True if the connection is alive, false otherwise.
         */
        static bool isWsAlive(WS* ws, Impl* impl) {
            if (!ws || !impl) return false;
            
            // Check atomic values
            if (!impl->running.load(std::memory_order_acquire)) return false;
            
            auto* ps = ws->getUserData();
            if (!ps || !ps->alive.load(std::memory_order_relaxed)) return false;
            
            // Check the connection set
            {
                std::shared_lock<std::shared_mutex> lock(impl->mx);
                return impl->conns.find(ws) != impl->conns.end();
            }
        }

        /**
         * @brief Safely sends a frame to the WebSocket, handling backpressure and queuing if needed.
         * @param ws Pointer to the WebSocket.
         * @param frame The frame data to send.
         * @return True if the send was initiated, false otherwise.
         */
        bool safeSend(WS* ws, std::vector<uint8_t> frame) {
            if (!ws) return false;

            auto* psd = ws->getUserData();
            if (!psd || !psd->loop) return false;
            
            // First, check the state
            if (!psd->state) return false;
            
            // Then, check the connection
            if (!Impl::isWsAlive(ws, this)) return false;

            auto impl = this;                          // capture ptr
            auto msg = std::move(frame);              // move payload
            bool compress = opts.enableCompression && msg.size() > opts.compressionThresholdBytes;

            try {
                // Attempt to send immediately
                psd->loop->defer([impl, ws, m = std::move(msg), compress]() mutable {
                    if (!Impl::isWsAlive(ws, impl)) return;
                    auto* current_psd = ws->getUserData();
                    if (!current_psd) return;

                    // Try sending directly
                    bool sent = false;
                    try {
                        sent = ws->send(std::string_view(
                            reinterpret_cast<const char*>(m.data()), m.size()),
                            uWS::BINARY, compress, true);
                    } catch (const std::exception& e) {
                        LOG_ERROR("safeSend direct send error: " + std::string(e.what()));
                    } catch (...) {
                        LOG_ERROR("safeSend direct send error: Unknown exception");
                    }

                    if (!sent) {
                        LOG_WARN("safeSend: uWS::send failed (backpressure?), queueing message.");
                        // C-1: Check sendQueue size before pushing
                        if (current_psd->sendQueue.size() >= impl->opts.maxSendQueueSize) {
                            LOG_ERROR("safeSend: Send queue full for WS, closing connection.");
                            ws->end(1009, "Send queue overflow"); // 1009 = Message Too Big
                            // No need to push to queue if we are closing
                        } else {
                            current_psd->sendQueue.push_back(std::move(m));
                        }
                    }
                });
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR("safeSend defer error: " + std::string(e.what()));
                return false;
            }
            catch (...) {
                LOG_ERROR("safeSend defer error: Unknown exception");
                return false;
            }
        }

        void onOpen(WS* ws, uWS::HttpRequest* req) {

        }
        void onMessage(WS* ws, std::string_view msg, uWS::OpCode op);
        void onDrain(WS* ws);
        void onClose(WS* ws, std::int32_t code, std::string_view message);

        /**
         * @brief Creates a protocol frame for the given type, id, and payload.
         * @param type The frame type.
         * @param id The message ID.
         * @param payload The payload data.
         * @return The constructed frame as a byte vector.
         */
        static std::vector<uint8_t> makeFrame(FrameType type, uint64_t id, const std::vector<uint8_t>& payload) {
            std::vector<uint8_t> buf;
            try {
                buf.reserve(1 + sizeof(id) + payload.size());
                
                // Frame type
                buf.push_back(static_cast<uint8_t>(type));
                
                // ID (network byte order - big endian)
                uint64_t networkId = hostToNetwork64(id);
                auto p = reinterpret_cast<const uint8_t*>(&networkId);
                buf.insert(buf.end(), p, p + sizeof(networkId));
                
                // Payload
                    buf.insert(buf.end(), payload.begin(), payload.end());
                
                // Debug log
                LOG_DEBUG("Created frame - Type: " + frameTypeToString(type) + 
                        ", ID: " + std::to_string(id) + 
                        ", Network ID: " + std::to_string(networkId) +  
                        ", Payload size: " + std::to_string(payload.size()) + 
                        ", Total size: " + std::to_string(buf.size()));
                return buf;
            } catch (const std::exception& e) {
                LOG_ERROR("makeFrame failed: " + std::string(e.what()));
                return {};  // Return empty buffer
            }
        }

        /**
         * @brief Sends a frame to the WebSocket according to the configured QoS level.
         * @param ws Pointer to the WebSocket.
         * @param payload The payload data to send.
         */
        void send(WS* ws, const std::vector<uint8_t>& payload) {
            if (!ws) return;
            
            // Validate WebSocket and its data
            if (!Impl::isWsAlive(ws, this)) return;
            
            auto* psd = ws->getUserData();
            if (!psd) return;
            
            if (opts.level == QoSLevel::AtLeastOnce)      sendQoS1(ws, payload);
            else if (opts.level == QoSLevel::ExactlyOnce) sendQoS2(ws, payload);
            else                                        rawSend(ws, payload);
        }

        /**
         * @brief Sends a frame to the WebSocket (alias for send).
         * @param ws Pointer to the WebSocket.
         * @param payload The payload data to send.
         */
        void sendFrame(WS* ws, const std::vector<uint8_t>& payload) {
            send(ws, payload);
        }

        /**
         * @brief Sends a QoS1 (AtLeastOnce) message to the WebSocket, with retry and tracking.
         * @param ws Pointer to the WebSocket.
         * @param payload The payload data to send.
         */
        void sendQoS1(WS* ws, const std::vector<uint8_t>& payload) {
            if (!ws) return;
            
            auto* psd = ws->getUserData();
            if (!psd) return;
            
            auto st = psd->state;
            if (!st) return;
            
            uint64_t id = st->nextId.fetch_add(1, std::memory_order_relaxed);
            auto frame = makeFrame(FRAME_DATA, id, payload);

            auto now = std::chrono::steady_clock::now();
            FrameInfo info;
            info.frame = std::move(frame);
            info.retryCount = 0;
            info.nextRetry = now + opts.backoffStrategy->nextDelay(1);

            // First, check the connection
            {
                std::shared_lock<std::shared_mutex> lock(mx);
                if (conns.find(ws) == conns.end()) {
                    LOG_WARN("WebSocket no longer in connection set");
                    return;
                }
            }

            // First, lock pendMx
            std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                st->pending1[id] = std::move(info);
                LOG_DEBUG("Added message id=" + std::to_string(id) + " to pending1 queue");

            // initial send
            if (!safeSend(ws, st->pending1[id].frame)) {
                LOG_ERROR("sendQoS1: Failed to send message id=" + std::to_string(id));
            }

        }

        /**
         * @brief Sends a QoS2 (ExactlyOnce) message to the WebSocket, with multi-stage delivery.
         * @param ws Pointer to the WebSocket.
         * @param payload The payload data to send.
         */
        void sendQoS2(WS* ws, const std::vector<uint8_t>& payload) {
            try {
                if (!ws) return;
                
                // Validate WebSocket and its data
                if (!Impl::isWsAlive(ws, this)) return;
                
                auto* psd = ws->getUserData();
                if (!psd || !psd->state) return;
                auto st = psd->state;
                uint64_t id = st->nextId.fetch_add(1, std::memory_order_relaxed);
                auto now_tp = std::chrono::steady_clock::now();

                // First, lock q2Mx
                std::unique_lock<std::shared_mutex> q2Lk(st->q2Mx);
                
                // Check if already in qos2Pending
                if (st->qos2Pending.count(id)) {
                    LOG_WARN("sendQoS2: Message ID " + std::to_string(id) + " already exists in qos2Pending. Skipping PREPARE frame send.");
                    return;
                }

                // Sonra pendMx'i kilitle
                {
                    std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                    if (st->pubPrepare.count(id) || st->pendingResp.count(id)) {
                        LOG_WARN("sendQoS2: Message ID " + std::to_string(id) + " already in ConnState pipeline (pubPrepare/pendingResp). Skipping.");
                        return;
                    }
                    st->pubPrepare[id] = payload;
                }

                Q2Meta m;
                m.stage = Q2Meta::Stage::PREPARE;
                m.frame = makeFrame(FRAME_PREPARE, id, {});
                m.retryCount = 0;
                m.nextRetry = now_tp + opts.backoffStrategy->nextDelay(0);
                m.lastTouched = now_tp;

                st->qos2Pending[id] = m;

                LOG_DEBUG("sendQoS2: Sending PREPARE for ID " + std::to_string(id));
                if (!safeSend(ws, m.frame)) {
                    LOG_ERROR("Failed to send PREPARE frame");
                    return;
                }
                
            }
            catch (const std::exception& e) {
                LOG_ERROR("Error in sendQoS2: " + std::string(e.what()));
                return;
            }
            catch (...) {
                LOG_ERROR("Unknown error in sendQoS2");
                return;
            }
        }


        /**
         * @brief Sends a raw message to the WebSocket without QoS handling.
         * @param ws Pointer to the WebSocket.
         * @param payload The payload data to send.
         */
        void rawSend(WS* ws, const std::vector<uint8_t>& payload) {
            if (!ws) return;
            
            // Validate WebSocket and its data
            if (!Impl::isWsAlive(ws, this)) return;
            
            auto* psd = ws->getUserData();
            if (!psd) return;
            
            if (!safeSend(ws, payload)) {
                LOG_ERROR("rawSend: Failed to send message");
            }
        }

        /**
         * @brief Checks and processes retries for pending QoS1 and QoS2 messages for a connection.
         * @param ws Pointer to the WebSocket.
         * @param now The current time point.
         */
        void checkAndProcessRetries(WS* ws, std::chrono::steady_clock::time_point now) {
            if (!ws || !ws->getUserData()) return;
            
            auto st = ws->getUserData()->state;
            if (!st) return;

            // QoS1 retries
            {
                std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                for (auto it = st->pending1.begin(); it != st->pending1.end();) {
                    if (now >= it->second.nextRetry) {
                        if (opts.maxRetry > 0 && it->second.retryCount >= opts.maxRetry) {
                            LOG_DEBUG("Max retries reached for QoS1 message id: " + std::to_string(it->first));
                            it = st->pending1.erase(it);
                            continue;
                        }
                        
                        if (safeSend(ws, it->second.frame)) {
                            ++it->second.retryCount;
                            it->second.nextRetry = now + opts.backoffStrategy->nextDelay(it->second.retryCount);
                            LOG_DEBUG("Retrying QoS1 message id: " + std::to_string(it->first) + 
                                     ", attempt: " + std::to_string(it->second.retryCount));
                        }
                        ++it;
                    } else {
                        ++it;
                    }
                }
            }

            // QoS2 retries
            {
                std::unique_lock<std::shared_mutex> q2Lk(st->q2Mx);
                for (auto it = st->qos2Pending.begin(); it != st->qos2Pending.end();) {
                    if (now >= it->second.nextRetry) {
                        if (opts.maxRetry > 0 && it->second.retryCount >= opts.maxRetry) {
                            LOG_DEBUG("Max retries reached for QoS2 message id: " + std::to_string(it->first));
                            it = st->qos2Pending.erase(it);
                            continue;
                        }

                        if (safeSend(ws, it->second.frame)) {
                            ++it->second.retryCount;
                            it->second.lastTouched = now;
                            it->second.nextRetry = now + opts.backoffStrategy->nextDelay(it->second.retryCount);
                            LOG_DEBUG("Retrying QoS2 message id: " + std::to_string(it->first) + 
                                     ", stage: " + std::to_string(static_cast<int>(it->second.stage)) +
                                     ", attempt: " + std::to_string(it->second.retryCount));
                        }
                        ++it;
                    } else {
                        ++it;
                    }
                }
            }
        }

        /**
         * @brief Periodically checks all connections for message retries and session cleanup.
         * @param stoken Stop token for thread interruption.
         */
        void retryLoop(std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                auto now = std::chrono::steady_clock::now();
                
                // Check retries for each connection
                {
                    std::shared_lock<std::shared_mutex> lock(mx);
                    for (auto* ws : conns) {
                        if (!ws || !ws->getUserData()) continue;
                        checkAndProcessRetries(ws, now);
                    }
                }
                
                // Session cleanup
                smgr.reap(clockMs());
                
                // Add a short wait
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        /**
         * @brief Retries sending pending QoS1 messages for a connection.
         * @param ws Pointer to the WebSocket.
         */
        void retry(WS* ws) {
            if (opts.level != QoSLevel::AtLeastOnce) return;
            
            // Add extra safety checks
            auto* psd = ws->getUserData();
            if (!psd) return;
            
            auto st = psd->state;
            if (!st) return;
            
            // First, check the connection
            {
                std::shared_lock<std::shared_mutex> lock(mx);
                if (conns.find(ws) == conns.end()) {
                    LOG_WARN("WebSocket no longer in connection set");
                    return;
                }
            }
            
            auto now = std::chrono::steady_clock::now();
            std::unique_lock<std::shared_mutex> pendLk(st->pendMx);

            for (auto it = st->pending1.begin(); it != st->pending1.end();) {
                if (st->seenSet.contains(it->first)) {     
                    it = st->pending1.erase(it);           // ACKed � clean up
                    continue;
                }
                auto& info = it->second;
                if (now < info.nextRetry) {
                    ++it; 
                    continue; 
                }
                if (opts.maxRetry && info.retryCount >= opts.maxRetry) {
                    it = st->pending1.erase(it);
                    continue;
                }

                if (!safeSend(ws, info.frame)) {
                    LOG_WARN("retry: Failed to send message id=" + std::to_string(it->first) + 
                            ", will retry later");
                    ++it;
                    continue;
                }

                ++info.retryCount;
                info.nextRetry = now + opts.backoffStrategy->nextDelay(info.retryCount);
                ++it;
            }
        }


        /**
         * @brief Registers a message ID as seen for duplicate detection, with TTL cleanup.
         * @param st Reference to the connection state.
         * @param id The message ID.
         * @param ttlMs Time-to-live in milliseconds.
         * @return True if the ID was newly registered, false if it was already seen.
         */
        static bool registerSeen(ConnState& st, uint64_t id, uint32_t ttlMs) {
            std::unique_lock<std::shared_mutex> pendLk(st.pendMx);
            auto now = std::chrono::steady_clock::now();
            auto ttl = std::chrono::milliseconds(ttlMs);
            while (!st.seenQ.empty() && now - st.seenQ.front().second > ttl) {
                st.seenSet.erase(st.seenQ.front().first);
                st.seenQ.pop_front();
            }
            auto [insIt, inserted] = st.seenSet.insert(id);
            if (inserted) st.seenQ.emplace_back(id, now);
            return inserted;
        }

        /**
         * @brief Flushes the outgoing message queue for a WebSocket connection.
         * @param ws Pointer to the WebSocket.
         * @param impl Pointer to the implementation instance.
         */
        static void flushQueue(WS* ws, Impl* impl) { // This is the correct one
            if (!ws || !impl) return;
            auto* psd = ws->getUserData();
            if (!psd || psd->sendQueue.empty()) return;

            if (!Impl::isWsAlive(ws, impl)) return; // Check if ws is still alive

            // C-2: Reduce log verbosity for frequent operations
            // LOG_DEBUG("flushQueue: Attempting to send " + std::to_string(psd->sendQueue.size()) + " queued messages."); 

            while (!psd->sendQueue.empty()) {
                std::vector<uint8_t> frame = std::move(psd->sendQueue.front());
                bool compress = impl->opts.enableCompression && frame.size() > impl->opts.compressionThresholdBytes;
                bool sent = false;
                try {
                     sent = ws->send(std::string_view(
                        reinterpret_cast<const char*>(frame.data()), frame.size()),
                        uWS::BINARY, compress, true);
                } catch (const std::exception& e) {
                    LOG_ERROR("flushQueue send error: " + std::string(e.what()));
                    // Do not pop, try again later or handle error differently
                    break; 
                } catch (...) {
                    LOG_ERROR("flushQueue send error: Unknown exception");
                    // Do not pop, try again later or handle error differently
                    break; 
                }

                if (sent) {
                    psd->sendQueue.pop_front();
                    // C-2: Reduce log verbosity
                    // LOG_DEBUG("flushQueue: Sent one message, " + std::to_string(psd->sendQueue.size()) + " remaining."); 
                } else {
                    LOG_WARN("flushQueue: uWS::send failed (backpressure?), stopping flush for now.");
                    // Message remains in queue to be sent later
                    break;
                }
            }
        }


        /**
         * @brief Handles session state changes (online/offline) and processes offline messages.
         * @param session The session whose state changed.
         * @param isOnline True if the session is now online, false if offline.
         */
        void handleSessionState(std::shared_ptr<Session> session, bool isOnline) {
            if (!session) {
                LOG_ERROR("Invalid session");
                return;
            }

            if (isOnline) {
                LOG_DEBUG("Session " + std::string(session->id()) + " is now ONLINE");
                // Process offline messages
                smgr.processOfflineMessages(
                    session->id(),
                    [this, session](const std::vector<uint8_t>& data) {
                        // Find the WebSocket
                        {
                            std::shared_lock<std::shared_mutex> lock(mx);
                            for (auto* ws : conns) {
                                if (ws && ws->getUserData() && 
                                    ws->getUserData()->session == session) {
                                    sendFrame(ws, data);
                                    return;
                                }
                            }
                        }
                    }
                );
            } else {
                LOG_DEBUG("Session " + std::string(session->id()) + " is now OFFLINE");
            }
        }

        // --- members ---
        SessionManager& smgr;
        uint16_t        idleTimeout;
        uint32_t     maxPay;
        ReliableOptions opts{ QoSLevel::None };
        std::set<WS*> conns;
        std::shared_mutex mx;
        std::jthread     retryTh;
        std::jthread serverTh;
        DataCallback    dataCb;
        SessionRegisterCallback regCb;
        DisconnectCallback      discCb;

        // new for flexible backoff
        std::atomic_bool running{false};

        std::atomic<bool> processingRetries{false}; // Flag added

        ~Impl() {
            running.store(false, std::memory_order_release);
        }
    };

    WebSocketTransport::WebSocketTransport(SessionManager& sm, uint16_t idle, uint32_t  maxPayloadBytes)
        : pImpl_(std::make_unique<Impl>(sm, idle, maxPayloadBytes)) {
            // Start session cleanup timer
            pImpl_->smgr.startCleanupTimer();
        }
    WebSocketTransport::~WebSocketTransport() { stop(); }

    void WebSocketTransport::setReliable(const ReliableOptions& o) {
        ReliableOptions newOpts = o;
        if (!newOpts.backoffStrategy) {
            newOpts.backoffStrategy = std::make_shared<ExponentialBackoff>(
                std::chrono::milliseconds(newOpts.baseRetryMs),
                std::chrono::milliseconds(newOpts.maxBackoffMs));
        }
        bool reset = newOpts.level != pImpl_->opts.level
            || newOpts.baseRetryMs != pImpl_->opts.baseRetryMs
            || newOpts.maxRetry != pImpl_->opts.maxRetry;
        if (reset) {
            {
                std::unique_lock<std::shared_mutex> lock(pImpl_->mx);
                for (auto* ws : pImpl_->conns) {
                    if (ws && ws->getUserData() && ws->getUserData()->state) { // Ensure psd and state are valid
                        auto st = ws->getUserData()->state;
                        std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                        st->pending1.clear();
                        st->seenSet.clear();
                        st->seenQ.clear();
                        st->pubPrepare.clear();
                        // qos2Pend is handled by q2mx, no need to clear here directly from ConnState
                    }
                }
            }
        }
        pImpl_->opts = std::move(newOpts);
    }

    void WebSocketTransport::setHandshakeInspector(std::shared_ptr<IHandshakeInspector> inspector) {
        pImpl_->inspector = std::move(inspector);
    }

    void WebSocketTransport::start(uint16_t port) {
        if (!pImpl_->retryTh.joinable()) {            
            pImpl_->retryTh = std::jthread([this](std::stop_token stoken) {
                pImpl_->retryLoop(stoken);
                });
        }

        if (!pImpl_->serverTh.joinable()) {
            pImpl_->serverTh = std::jthread([this, port](std::stop_token stoken) {
                // 1) UWS::App nesnesi
                uWS::App app{};

                // 2) Create the Behavior object
                using Behavior = uWS::TemplatedApp<false>::WebSocketBehavior<PerSocketData>;
                Behavior wsBeh{};

                // 3) Set the settings one by one
                wsBeh.maxPayloadLength = pImpl_->maxPay;
                wsBeh.idleTimeout = pImpl_->idleTimeout;


                wsBeh.upgrade = [this](auto* res, auto* req, auto* ctx)
                {
                    /* 1) ���  Create identity with inspector  ��� */
                    auto identity = getInspector()->extract(*req);
                    if (!identity) {
                        LOG_ERROR("Handshake inspection failed: " + pImpl_->inspector->rejectReason());
                        res->writeStatus("400 Bad Request")->end(pImpl_->inspector->rejectReason());
                        return;
                    }
                    if (!pImpl_->running.load()) {
                        res->writeStatus("503 Service Unavailable")->end("Server stopping");
                        return;
                    }


                    /* 2) ���  Request session from SessionManager  ��� */
                    std::uint64_t nowMs = clockMs();  // use steady_clock
                    auto sess = pImpl_->smgr.getOrCreate(*identity, nowMs);

                    if (auto* oldWs = reinterpret_cast<WS*>(sess->liveWs())) {
                        if (Impl::isWsAlive(oldWs, pImpl_.get())) {
                            LOG_DEBUG("Closing existing connection for client: " + identity->clientId);
                            if (auto* ps = oldWs->getUserData(); ps && ps->loop) {
                                ps->loop->defer([oldWs] {
                                    oldWs->end(1000, "Connection replaced by new client");
                                    });
                            }
                        }
                    }

                    std::string dbgMsg = "Session created/found - id: " + std::string(sess->id());
                    LOG_DEBUG(dbgMsg);

                    /* 3) ���  Prepare PerSocketData  ��� */
                    PerSocketData psd;
                    psd.session = sess;
                    psd.state = sess->qosState;
                    psd.lastActive = std::chrono::steady_clock::now();


                    /* 4) ���  Add current token to 101 response  ��� */
                    std::string tokenHex = binaryrpc::toHex(sess->identity().sessionToken);


                    /* 5) ���  uWS upgrade (HTTP ? WS)   ��� */
                    const std::string_view wsKey = req->getHeader("sec-websocket-key");
                    if (wsKey.empty()) {
                        LOG_ERROR("Missing Sec-WebSocket-Key header");
                        res->writeStatus("400 Bad Request")->end("Missing Sec-WebSocket-Key");
                        return;
                    }


                    try {
                        LOG_DEBUG("Attempting WebSocket upgrade");
                        
                        // Perform the upgrade - this will automatically add 101 Switching Protocols
                        // and other WebSocket headers
                        // tokenHex is already prepared
                            res->cork([res,
                                       psd,
                                       wsKey = std::string_view{ req->getHeader("sec-websocket-key") },
                                       protocol = std::string_view{ req->getHeader("sec-websocket-protocol") },
                                       extensions = std::string_view{ req->getHeader("sec-websocket-extensions") },
                                       ctx,
                                       tokenHex = std::move(tokenHex)
                            ]() mutable {
                                res->writeStatus("101 Switching Protocols")          // correct status-line
                                    ->writeHeader("x-session-token", tokenHex)        // ***only*** custom header
                                    ->template upgrade<PerSocketData>(                         // uWS adds its own Upgrade + Connection
                                        std::move(psd),
                                        wsKey,
                                        protocol,
                                        extensions,
                                        ctx
                                        );
                            });
                        
                        LOG_DEBUG("WebSocket upgrade successful");
                    }
                    catch (const std::exception& ex) {
                        LOG_ERROR("WebSocket upgrade failed: " + std::string(ex.what()));
                        try {
                            res->writeStatus("500 Internal Server Error")->end("Upgrade failed");
                        }
                        catch (...) {/* ignore � socket closed */ }
                    }
                };

                /* After the connection is opened, we only receive WS* */
                wsBeh.open = [this](WS* ws) {
                    ws->getUserData()->loop = uWS::Loop::get();
                    auto* ps = ws->getUserData();
                    auto session = ps->session;

                    /* 1. Bind the socket to the session */
                    session->rebind(reinterpret_cast<Session::WS*>(ws));

                    auto st = session->qosState;
                    {
                        std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                        LOG_INFO("Replay open: sid= " +
                                 std::string(session->id()) + ", pending1= " + std::to_string(st->pending1.size()));
                    }

                    /* 2. Add to transport list */
                    {
                        std::unique_lock<std::shared_mutex> lock(pImpl_->mx);
                        pImpl_->conns.insert(ws);
                    }

                    session->connectionState = ConnectionState::ONLINE;
                    pImpl_->handleSessionState(session, true);

                    if (pImpl_->regCb)
                        pImpl_->regCb(session->id(), session);
                };



                wsBeh.message = [this](WS* ws, std::string_view msg, uWS::OpCode op) {
                    if (op != uWS::OpCode::BINARY) {
                        LOG_DEBUG("Received non-binary message, ignoring");
                        return;
                    }
                    
                    constexpr size_t hdr = 1 + sizeof(uint64_t);
                    if (msg.size() < hdr) {
                        std::string errorMsg = "Message too short: " + std::to_string(msg.size()) + " bytes";
                        LOG_ERROR(errorMsg);
                        return;
                    }

                    auto st = ws->getUserData()->state;
                    uint64_t networkId;
                    std::memcpy(&networkId, msg.data() + 1, sizeof networkId);
                    uint64_t id = networkToHost64(networkId);
                    auto raw = reinterpret_cast<const uint8_t*>(msg.data());
                    auto frameType = (FrameType)raw[0];

                    switch (frameType) {
                    case FRAME_ACK: {
                        //std::string debugMsg = "Processing FRAME_ACK for message id: " + std::to_string(id);
                        // First, check the connection
                        {
                            std::shared_lock<std::shared_mutex> lock(pImpl_->mx);
                            if (pImpl_->conns.find(ws) == pImpl_->conns.end()) {
                                LOG_WARN("WebSocket no longer in connection set");
                                return;
                            }
                        }
                        
                        // Then, lock pendMx
                        std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                        auto it = st->pending1.find(id);
                        if (it != st->pending1.end()) {
                            //std::string debugMsg = "Found message in pending1, removing id: " + std::to_string(id);
                            st->pending1.erase(it);
                        } else {
                            std::string warnMsg = "Received ACK for unknown message id: " + std::to_string(id);
                            LOG_WARN(warnMsg);
                        }
                        return;
                    }
                    case FRAME_DATA: {
                        std::string dbgMsg = "Processing FRAME_DATA for message id: " + std::to_string(id);
                        //LOG_DEBUG(dbgMsg);
                        std::vector<uint8_t> payload(raw + hdr, raw + msg.size());
                        std::string dbgMsg2 = "Payload size: " + std::to_string(payload.size()) + " bytes";
                        //LOG_DEBUG(dbgMsg2);

                        auto session = ws->getUserData()->session;
                        using namespace std::chrono;
                        const milliseconds ttl{pImpl_->opts.duplicateTtlMs};   // use pImpl_->opts

                        // Check if the RPC call is a duplicate
                        const bool isNewRpc = session        // NULL safety (theoretical)
                            ? session->acceptDuplicate(payload, ttl)  // Check according to RPC payload
                            : true;                       // if session is NULL, every RPC is "fresh"

                        std::string dbgMsg3 = "RPC is new: " + std::string(isNewRpc ? "true" : "false");
                        //LOG_DEBUG(dbgMsg3);

                        if (!pImpl_->dataCb) {
                            LOG_ERROR("[WebSocketTransport] dataCb is null! Protocol or callback missing.");
                            return;
                        }

                        if (isNewRpc && pImpl_->dataCb) {      // Only process the first RPC call
                            try {
                                //LOG_DEBUG("Calling dataCb with payload");
                                pImpl_->dataCb(payload, session, ws);
                                //LOG_DEBUG("dataCb completed successfully");
                            } catch (const std::exception& ex) {
                                LOG_ERROR("[WebSocketTransport] Exception in dataCb: " + std::string(ex.what()));
                                return;
                            }
                        }
                        return;
                    }
                    
                    case FRAME_PREPARE_ACK: {
                        LOG_DEBUG("Processing FRAME_PREPARE_ACK for message id: " + std::to_string(id));
                        auto now = std::chrono::steady_clock::now();
                        
                        // First, lock q2Mx
                        std::unique_lock<std::shared_mutex> q2Lk(st->q2Mx);
                        auto it = st->qos2Pending.find(id);
                        if (it != st->qos2Pending.end() && it->second.stage == Q2Meta::Stage::PREPARE) {
                            // First, update the session state
                            {
                                std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                                auto it2 = st->pubPrepare.find(id);
                                if (it2 != st->pubPrepare.end()) {
                                    st->pendingResp[id] = std::move(it2->second);
                                    st->pubPrepare.erase(it2);
                                }
                            }


                            // Then, update the transport state
                            it->second.stage = Q2Meta::Stage::COMMIT;
                            it->second.frame = Impl::makeFrame(FRAME_COMMIT, id, {});
                            it->second.retryCount = 0;
                            it->second.nextRetry = now + pImpl_->opts.backoffStrategy->nextDelay(0);

                            // Send the COMMIT frame with safeSend
                            if (!pImpl_->safeSend(ws, it->second.frame)) {
                                LOG_ERROR("Failed to send COMMIT frame");
                                return;
                            }

                            LOG_DEBUG("Scheduled first COMMIT retry for ID " + std::to_string(id));
                        } else {
                            LOG_WARN("No matching PREPARE found for id: " + std::to_string(id));
                        }
                        return;
                    }

                    case FRAME_COMPLETE: {
                        LOG_DEBUG("Processing FRAME_COMPLETE for message id: " + std::to_string(id));
                        
                        // First, lock q2Mx
                        std::unique_lock<std::shared_mutex> q2Lk(st->q2Mx);

                        st->qos2Pending.erase(id);
                       
                        // Then, lock pendMx
                        std::unique_lock<std::shared_mutex> pendLk(st->pendMx);
                        auto it = st->pendingResp.find(id);
                        if (it != st->pendingResp.end()) {
                            auto dataFrame = Impl::makeFrame(FRAME_DATA, id, it->second);
                            pImpl_->safeSend(ws, dataFrame);
                            st->pendingResp.erase(it);
                        }
                        return;
                    }

                    default:
                        std::string warnMsg = "Unknown frame type: " + frameTypeToString(frameType);
                        LOG_WARN(warnMsg);
                        break;
                    }

                    if (pImpl_->opts.level == QoSLevel::None && pImpl_->dataCb) {
                        std::vector<uint8_t> rawData(msg.size());
                        std::memcpy(rawData.data(), raw, msg.size());
                        pImpl_->dataCb(rawData, ws->getUserData()->session, ws);
                    }
                };

                wsBeh.drain = [this](WS* ws) { // Capture 'this' to access pImpl_
                    Impl::flushQueue(ws, pImpl_.get()); // Pass pImpl_ for opts access
                };

                wsBeh.close = [this](WS* ws, int code, std::string_view reason) {
                    LOG_DEBUG("WebSocket close event - code: " + std::to_string(code) + ", reason: " + std::string(reason));
                    
                    // First mark the connection as not alive to prevent any new operations
                    if (auto* ps = ws->getUserData()) {
                        ps->alive.store(false, std::memory_order_relaxed);
                    }
                    
                    auto* ps = ws->getUserData();
                    if (!ps) {
                        LOG_DEBUG("WebSocket close - no PerSocketData found");
                        return;  // Already cleaned up or invalid
                    }
                    
                    auto session = ps->session;
                    if (!session) {
                        LOG_DEBUG("WebSocket close - no session attached");
                        return;  // No session attached
                    }
                    
                    LOG_DEBUG("WebSocket close - session: " + std::string(session->id()));
                    
                    {
                        std::unique_lock<std::shared_mutex> lock(pImpl_->mx);
                        pImpl_->conns.erase(ws);
                    }

                    const auto& cid = session->identity();
                    bool alive = false;
                    
                    {
                        std::shared_lock<std::shared_mutex> lock(pImpl_->mx);
                        alive = std::ranges::any_of(pImpl_->conns.begin(), pImpl_->conns.end(),
                            [cid](WS* other) {
                                return other && other->getUserData() && 
                                       other->getUserData()->session && 
                                       other->getUserData()->session->identity() == cid;
                            });
                    }

                    if (!alive) {
                        LOG_DEBUG("WebSocket close - marking session OFFLINE: " + std::string(session->id()));
                        session->connectionState = ConnectionState::OFFLINE;
                        pImpl_->handleSessionState(session, false);
                        session->rebind(nullptr);               // socket is gone
                        session->expiryMs = clockMs() + pImpl_->opts.sessionTtlMs;  // TTL
                    }

                    if (pImpl_->discCb) {
                        pImpl_->discCb(session);
                    }
                };

                app.ws<PerSocketData>("/*", std::move(wsBeh))
                    .listen(port, [port](auto* tok) {
                    if (tok)
                        std::cout << "[WS] listening " << port << "\n";
                    else
                        std::cerr << "bind fail\n";
                        });

                app.run();
                });
        }
    }

    std::shared_ptr<IHandshakeInspector> WebSocketTransport::getInspector() {
        if (!pImpl_->inspector) {
            LOG_WARN("Inspector was null, fallbacking to DefaultInspector");
            pImpl_->inspector = std::make_shared<DefaultInspector>();
        }
        return pImpl_->inspector;
    }


    void WebSocketTransport::stop() {
        if (pImpl_->retryTh.joinable()) {
            pImpl_->retryTh.request_stop();
            pImpl_->retryTh.join();
        }

        if (pImpl_->serverTh.joinable()) {
            pImpl_->serverTh.request_stop();
            pImpl_->serverTh.join();
        }

        pImpl_.reset();
    }

    void WebSocketTransport::send(const std::vector<uint8_t>& data) {
        std::shared_lock<std::shared_mutex> lock(pImpl_->mx);
        for (auto* ws : pImpl_->conns) {
            pImpl_->sendFrame(ws, data);
        }
    }
    void WebSocketTransport::sendToClient(void* conn, const std::vector<uint8_t>& data) {
        if (auto* ws = static_cast<WS*>(conn)) pImpl_->sendFrame(ws, data);
    }

    void WebSocketTransport::sendToSession(std::shared_ptr<Session> session, const std::vector<uint8_t>& data) {
        if (!session) {
            LOG_ERROR("Invalid session");
            return;
        }

        // Check the session's connection state
        if (session->connectionState == ConnectionState::OFFLINE) {
            // If offline, add to queue
            if (!pImpl_->smgr.addOfflineMessage(session->id(), data)) {
                LOG_WARN("Failed to queue offline message for session: " + session->id());
            }
            return;
        }

        // If online, find the WebSocket and send
        WS* targetWs = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock(pImpl_->mx);
            for (auto* ws : pImpl_->conns) {
                if (ws && ws->getUserData() && 
                    ws->getUserData()->session == session) {
                    targetWs = ws;
                    break;
                }
            }
        }

        // If WebSocket is found and still valid, send
        if (targetWs && Impl::isWsAlive(targetWs, pImpl_.get())) {
            pImpl_->sendFrame(targetWs, data);
        } else {
            // If WebSocket is invalid or not found, add to offline queue
            session->connectionState = ConnectionState::OFFLINE;
            if (!pImpl_->smgr.addOfflineMessage(session->id(), data)) {
                LOG_WARN("Failed to queue offline message for session: " + session->id());
            }
        }
    }
    void WebSocketTransport::disconnectClient(void* conn) {
        if (auto* ws = static_cast<WS*>(conn)) ws->close();
    }

#ifdef BINARYRPC_TEST
    void WebSocketTransport::onRawFrameFromClient(const std::vector<uint8_t>& frame)
    {
        if (sendInterceptor_) {
            sendInterceptor_(frame);
        }
        constexpr std::size_t hdr = 1 + sizeof(uint64_t);
        if (frame.size() <= hdr) return;

        auto session = pImpl_->smgr.createSession(
            ClientIdentity{"test", 0, {}},  // Simple identity for test
            clockMs()                   // Current time
        );
        if (pImpl_->regCb) pImpl_->regCb(session->id(), session);

        std::vector<uint8_t> raw(frame.begin() + hdr, frame.end());   // "echo:42"
        if (pImpl_->dataCb) pImpl_->dataCb(raw, session, nullptr);
    }

    void WebSocketTransport::setSendInterceptor(
        std::function<void(const std::vector<uint8_t>&)> f)
    {
        sendInterceptor_ = std::move(f);
    }

    std::vector<uint8_t> WebSocketTransport::test_makeFrame(FrameType type, uint64_t id, const std::vector<uint8_t>& payload) {
        return Impl::makeFrame(type, id, payload);
    }

    bool WebSocketTransport::test_registerSeen(void* connStatePtr, uint64_t id, uint32_t ttlMs) {
        auto& st = *reinterpret_cast<ConnState*>(connStatePtr);
        return Impl::registerSeen(st, id, ttlMs);
    }


#endif

    void WebSocketTransport::setCallback(DataCallback cb) { pImpl_->dataCb = std::move(cb); }
    void WebSocketTransport::setSessionRegisterCallback(SessionRegisterCallback cb) { pImpl_->regCb = std::move(cb); }
    void WebSocketTransport::setDisconnectCallback(DisconnectCallback cb) { pImpl_->discCb = std::move(cb); }
}
