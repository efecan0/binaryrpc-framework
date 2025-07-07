/**
 * @file iplugin.hpp
 * @brief Interface for plugins in BinaryRPC.
 *
 * Defines the IPlugin interface for implementing custom plugins that can be loaded
 * into the BinaryRPC framework.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once

namespace binaryrpc {

    /**
     * @class IPlugin
     * @brief Interface for custom plugins in BinaryRPC.
     *
     * Implement this interface to provide custom plugin functionality for the framework.
     */
    class IPlugin {
    public:
        virtual ~IPlugin() = default;
        /**
         * @brief Initialize the plugin. Called when the plugin is loaded.
         */
        virtual void initialize() = 0;
        /**
         * @brief Get the name of the plugin.
         * @return Name of the plugin as a C-string
         */
        virtual const char* name() const = 0;
    };

}