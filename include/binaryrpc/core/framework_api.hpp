/**
 * @file framework_api.hpp
 * @brief FrameworkAPI provides helper functions for accessing and manipulating sessions from outside the core.
 *
 * This API allows sending data to sessions, disconnecting sessions, listing session IDs, and managing session state fields.
 * It acts as a bridge between the core session/transport management and user-level logic.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <optional>

#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"

namespace binaryrpc {

    /**
     * @class FrameworkAPI
     * @brief Helper API for accessing and manipulating sessions from outside the core.
     *
     * Provides methods for sending data, disconnecting sessions, listing session IDs, and managing session state fields.
     */
    class FrameworkAPI {
    public: 
        /**
         * @brief Construct a FrameworkAPI instance.
         * @param sm Pointer to the SessionManager
         * @param tr Pointer to the ITransport instance
         */
        FrameworkAPI(SessionManager* sm, ITransport* tr);

        /**
         * @brief Send data to a session by session ID.
         * @param sid Session ID
         * @param data Data to send
         * @return True if the data was sent successfully, false otherwise
         */
        bool sendTo(const std::string& sid, const std::vector<uint8_t>& data) const;

        /**
         * @brief Send data to a specific session object.
         * @param session Shared pointer to the session
         * @param data Data to send
         */
        void sendToSession(std::shared_ptr<Session> session, const std::vector<uint8_t>& data);

        /**
         * @brief Disconnect a session by session ID.
         * @param sid Session ID
         * @return True if the session was disconnected, false otherwise
         */
        bool disconnect(const std::string& sid) const;

        /**
         * @brief List all active session IDs.
         * @return Vector of session ID strings
         */
        std::vector<std::string> listSessionIds() const;

        /**
         * @brief Set a custom field for a session.
         * @tparam T Type of the value
         * @param sid Session ID
         * @param key Field key
         * @param value Value to set
         * @param indexed Whether the field should be indexed for fast lookup
         * @return True if the field was set successfully, false otherwise
         */
        template<typename T>
        bool setField(const std::string& sid,
            const std::string& key,
            const T& value,
            bool indexed = false);

        /**
         * @brief Get a custom field value from a session.
         * @tparam T Type of the value
         * @param sid Session ID
         * @param key Field key
         * @return Optional containing the value if present, std::nullopt otherwise
         */
        template<typename T>
        std::optional<T> getField(const std::string& sid,
            const std::string& key) const;

        /**
         * @brief Find sessions by a field key and value.
         * @param key Field key to search by
         * @param value Value to match
         * @return Vector of shared pointers to matching sessions
         */
        std::vector<std::shared_ptr<Session>>
            findBy(const std::string& key,
                const std::string& value) const;

    private:
        SessionManager* sm_; ///< Pointer to the SessionManager
        ITransport* tr_;     ///< Pointer to the ITransport instance
    };

}
