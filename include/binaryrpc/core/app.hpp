/**
 * @file app.hpp
 * @brief Main application (App) class and entry point for the BinaryRPC framework.
 *
 * Handles application startup, transport and protocol configuration, middleware and plugin management,
 * and other core functions. Designed as a singleton.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "binaryrpc/core/types.hpp"
#include <vector>
#include <string>
#include <memory>

namespace folly { // Forward declaration
    class CPUThreadPoolExecutor;
}

namespace binaryrpc {

    // Forward declarations for public interfaces and types
    class ITransport;
    class IPlugin;
    class IProtocol;
    class SessionManager; // This will be opaque

    /**
     * @class App
     * @brief Main application class (singleton) for the BinaryRPC framework.
     *
     * Handles application startup, transport and protocol configuration, middleware and plugin management,
     * and other core functions. Serves as the main entry point for the entire framework.
     */
    class App {
    public:
        /**
         * @brief Get the singleton instance.
         * @return Reference to the single App instance
         */
        static App& getInstance();

        // Delete copy and assignment operators
        App(const App&) = delete;
        App& operator=(const App&) = delete;

        /**
         * @brief Destructor for the App class.
         */
        ~App();

        /**
         * @brief Start the application on the specified port.
         * @param port Port number to listen on
         */
        void run(uint16_t port);
        /**
         * @brief Stop the application.
         */
        void stop();
        /**
         * @brief Set the transport layer.
         * @param transport Transport instance to use (e.g., WebSocket)
         */
        void setTransport(std::unique_ptr<ITransport> transport);
        /**
         * @brief Add and initialize a plugin.
         * @param plugin Plugin instance to add
         */
        void usePlugin(std::unique_ptr<IPlugin> plugin);
        /**
         * @brief Add a global middleware.
         * @param mw Middleware function to add
         */
        void use(Middleware mw);
        /**
         * @brief Add middleware for a specific method.
         * @param method Target method name
         * @param mw Middleware function to add
         */
        void useFor(const std::string& method, Middleware mw);
        /**
         * @brief Add middleware for multiple methods.
         * @param methods List of target method names
         * @param mw Middleware function to add
         */
        void useForMulti(const std::vector<std::string>& methods, Middleware mw);
        /**
         * @brief Register a new RPC method.
         * @param method Name of the method
         * @param handler Handler function to execute when the RPC is called
         */
        void registerRPC(const std::string& method, RpcContextHandler handler);
        /**
         * @brief Get the active transport instance.
         * @return Pointer to the ITransport instance
         */
        ITransport* getTransport() const;
        /**
         * @brief Get a reference to the SessionManager.
         * @return Reference to the SessionManager
         */
        SessionManager& getSessionManager();
        /**
         * @brief Get a const reference to the SessionManager.
         * @return Const reference to the SessionManager
         */
        const SessionManager& getSessionManager() const;
        /**
         * @brief Set the protocol.
         * @param proto Protocol instance to use
         */
        void setProtocol(std::shared_ptr<IProtocol> proto);
        /**
         * @brief Get the active protocol instance.
         * @return Pointer to the IProtocol instance
         */
        IProtocol* getProtocol() const;

    private:
        App(); // Private constructor for singleton

        // PIMPL idiom
        struct Impl;
        std::unique_ptr<Impl> pImpl_;
    };
}
