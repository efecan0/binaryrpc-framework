/**
 * @file room_plugin.hpp
 * @brief RoomPlugin for group communication and room management in BinaryRPC.
 *
 * Provides a plugin for managing rooms (groups of sessions), allowing users to join, leave,
 * broadcast messages, and query room membership. Thread-safe and integrated with the session manager.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once

#include "binaryrpc/core/interfaces/iplugin.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <vector>

namespace binaryrpc {

    /**
     * @class RoomPlugin
     * @brief Plugin for managing rooms (groups of sessions) in BinaryRPC.
     *
     * Allows sessions to join/leave rooms, broadcast messages to all members,
     * and query the list of members in a room. Thread-safe and integrated with the session manager.
     */
    class RoomPlugin : public IPlugin {
    public:
        /**
         * @brief Construct a RoomPlugin with a session manager and transport.
         * @param sessionManager Reference to the SessionManager
         * @param transport Pointer to the transport
         */
        explicit RoomPlugin(SessionManager& sessionManager, ITransport* transport);
        /**
         * @brief Destructor for RoomPlugin.
         */
        ~RoomPlugin() override = default;

        /**
         * @brief Initialize the plugin. Called when the plugin is loaded.
         */
        void initialize() override;
        /**
         * @brief Get the name of the plugin.
         * @return Name of the plugin as a C-string
         */
        const char* name() const override { return "RoomPlugin"; }

        /**
         * @brief Add a session to a room.
         * @param room Name of the room
         * @param sid Session ID to add
         */
        void join(const std::string& room, const std::string& sid);
        /**
         * @brief Remove a session from a room.
         * @param room Name of the room
         * @param sid Session ID to remove
         */
        void leave(const std::string& room, const std::string& sid);
        /**
         * @brief Remove a session from all rooms it belongs to.
         * @param sid Session ID to remove
         */
        void leaveAll(const std::string& sid);
        /**
         * @brief Broadcast a message to all members of a room.
         * @param room Name of the room
         * @param data Data to broadcast
         */
        void broadcast(const std::string& room, const std::vector<uint8_t>& data);
        /**
         * @brief Get the list of session IDs in a room.
         * @param room Name of the room
         * @return Vector of session IDs in the room
         */
        std::vector<std::string> getRoomMembers(const std::string& room) const;

    private:
        SessionManager& sessionManager_; ///< Reference to the session manager
        ITransport* transport_; ///< Pointer to the transport
        std::unique_ptr<std::mutex> mtx_; ///< Mutex for thread safety
        std::unordered_map<std::string, std::unordered_set<std::string>> rooms_; ///< Room membership map
    };

}