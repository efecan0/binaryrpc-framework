﻿/**
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
#include <string>
#include <vector>
#include <memory>

namespace binaryrpc {

    // Forward declarations
    class SessionManager;
    class ITransport;

    /**
     * @class RoomPlugin
     * @brief A plugin for managing chat rooms or game lobbies.
     *
     * Provides functionality for joining, leaving, and broadcasting messages to rooms.
     */
    class RoomPlugin : public IPlugin {
    public:
        /**
         * @brief Construct a RoomPlugin.
         * @param sessionManager Reference to the SessionManager
         * @param transport Pointer to the ITransport instance
         */
        RoomPlugin(SessionManager& sessionManager, ITransport* transport);
        
        /**
         * @brief Destructor for RoomPlugin.
         */
        ~RoomPlugin() override;

        /**
         * @brief Initialize the plugin (called by App).
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
        // PIMPL idiom
        struct Impl;
        std::unique_ptr<Impl> pImpl_;
    };

}