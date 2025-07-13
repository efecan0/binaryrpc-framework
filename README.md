# BinaryRPC

## 🧭 Motivation

While working at my company, I had previously developed a WebSocket server prototype in Java. However, over time, we started to experience performance issues. This led me to turn to C++, a language that offers more speed and low-level system control. Through my research, I discovered that uWebSockets was one of the best options in terms of performance, so I began developing with this library.

However, since uWebSockets is a very "core" library, I had to handle many details myself. Even adding a simple feature required a lot of infrastructure management. During this process, I explored other libraries in the C++ ecosystem, but most were either too heavy or did not prioritize developer experience.

Having developed many projects with Node.js and Express.js in the past, I had this idea:
Why not have a modern RPC framework in C++ that supports middleware and session logic, just like Express.js?

With this idea in mind, I started designing a system that includes session management, a middleware structure, and an RPC handler architecture. At first, it was just a dream, but over time I built the building blocks of that dream one by one. I laid the foundations of this framework before graduating, and after graduation, I decided to make it open source as both a gift to myself and a contribution to the developer community.

Although I sometimes got lost in the vast control that C++ offers, I progressed step by step to bring it to its current state. I tried to include the essential things a developer might need. I hope you enjoy using it and encounter no issues, because this project is, in fact, a heartfelt contribution that a newly graduated engineer wanted to offer to the world.

In the future, I aim to evolve this architecture into an actor-mailbox model. However, I believe the current structure needs to be further solidified first. If you have any suggestions or contributions regarding this process, please feel free to contact me.

---

> **BinaryRPC** is a high‑throughput RPC framework built on top of **uWebSockets**.

> It is designed for latency-sensitive applications such as multiplayer games, financial tick streams, and IoT dashboards, delivering ultra-low latency and minimal overhead. With a modular architecture and efficient networking, BinaryRPC remains lightweight both in resource usage and developer experience.




 
---

  

## ✨ Highlights

  

| Capability | Description |
| -- | -- |
| ⚡ **WebSocket transport** | Blazing‑fast networking powered by uWebSockets and `epoll`/`kqueue`. |
| 🔄 **Configurable reliability (QoS)** | `None`, `AtLeastOnce`, `ExactlyOnce` with retries, ACKs & two‑phase commit, plus **pluggable back‑off strategies** & **per‑session TTL**. |
| 🧩 **Pluggable layers** | Drop‑in protocols (`SimpleText`, **MsgPack**, …), transports, middleware & plugins. |
| 🧑‍🤝‍🧑 **Stateful sessions** | Reconnect‑friendly `Session` objects with automatic expiry & indexed fields. |
| 🛡️ **Middleware chain** | JWT auth, token bucket rate‑limiter and any custom middleware you write. |
| 🔌 **Header‑only core** | Almost all of BinaryRPC lives in headers – *just add include path*. |

---

This project is a modern C++ RPC framework with several external dependencies. To use it, you should first install the required dependencies (see below), then build the project, and finally link it in your own project. The example_server directory demonstrates typical usage.
### 1. Prerequisites

- **CMake**: Version 3.16 or higher.
- **C++ Compiler**: A compiler with C++20 support (e.g., MSVC, GCC, Clang).
- **vcpkg**: A C++ package manager from Microsoft, used for dependencies.
- **Git**: For cloning the repository.

### 2. Building and Installing the Library

> **Note:**
> Starting from version 0.1.0, BinaryRPC expects you to set the `VCPKG_ROOT` environment variable to your vcpkg installation path **on Windows**. This makes the build process portable and avoids hardcoding paths. If you do not set this variable on Windows, CMake will stop with an error.
>
> **Linux/macOS users:** You don't need to set VCPKG_ROOT as the project uses system package managers on these platforms.
>
> **How to set VCPKG_ROOT (Windows only):**
> - **Windows (PowerShell):**
>   ```powershell
>   $env:VCPKG_ROOT = "C:/path/to/vcpkg"
>   ```
> - **Windows (CMD):**
>   ```cmd
>   set VCPKG_ROOT=C:/path/to/vcpkg
>   ```
>
> Replace `/path/to/vcpkg` with the actual path where you cloned vcpkg.

#### Step 2.1: Install Dependencies with vcpkg

First, ensure you have [vcpkg](https://github.com/microsoft/vcpkg) installed and bootstrapped. Then, install all required dependencies:

# Install all dependencies for binaryrpc

## Core Dependencies

These are required for building and running the core framework:

### Windows (vcpkg)
```bash
./vcpkg install unofficial-uwebsockets zlib folly glog gflags fmt double-conversion --triplet x64-windows
```

### Linux (Arch/pacman)
```bash
sudo pacman -S uwebsockets zlib folly glog gflags fmt double-conversion openssl usockets
```

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install libuwebsockets-dev zlib1g-dev libfolly-dev libgoogle-glog-dev libgflags-dev libfmt-dev libdouble-conversion-dev libssl-dev libusockets-dev
```

> **Note on folly and glog:**
> - On some distributions, the package names may differ or the packages may not be available in the default repositories. In that case, you may need to build [folly](https://github.com/facebook/folly) and [glog](https://github.com/google/glog) from source. Please refer to their official documentation for build instructions.
> - **Folly and glog can sometimes be incompatible on certain Linux systems.** If you encounter build errors related to glog, you can try disabling glog or ensure you are using compatible versions. On Linux, BinaryRPC disables glog logging by default if there is a known incompatibility.
> - If you see an error like `folly library not found!`, make sure folly is installed and the library path is visible to the linker (e.g., `/usr/lib` or `/usr/local/lib`).
> - If you see an error like `glog library not found!`, make sure glog is installed and the library path is visible to the linker.

## Optional Dependencies

Install these only if you need the corresponding features:

- **JWT-based authentication middleware:**
  - Windows: `./vcpkg install jwt-cpp --triplet x64-windows`
  - Linux: `sudo pacman -S jwt-cpp`
- **JSON payload support (e.g., for nlohmann-json):**
  - Windows: `./vcpkg install nlohmann-json --triplet x64-windows`
  - Linux: `sudo pacman -S nlohmann-json`

> You only need to install these optional dependencies if you plan to use JWT authentication or JSON-based payloads in your application or middleware.

#### Step 2.2: Configure, Build, and Install BinaryRPC

This process will compile the library and install its headers and binaries into a standard system location, making it available for other projects.

```bash
# 1. Clone the repository
git clone https://github.com/efecan0/binaryrpc-framework.git
cd binaryrpc

# 2. Create a build directory
cmake -E make_directory build
cd build

# 3. Configure the project with the vcpkg toolchain
#    Use the VCPKG_ROOT environment variable for portability
cmake .. -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

# 4. Build the library
cmake --build . --config Release

# 5. Install the library
#    On Linux/macOS, you may need to run this with sudo
cmake --install . --config Release
```

**For Linux/macOS users:**

```bash
# 1. Clone the repository
git clone https://github.com/efecan0/binaryrpc-framework.git
cd binaryrpc

# 2. Create a build directory
cmake -E make_directory build
cd build

# 3. Configure the project (no toolchain file needed on Linux/macOS)
cmake ..

# 4. Build the library
cmake --build . --config Release

# 5. Install the library
#    You may need to run this with sudo
cmake --install . --config Release
```

After this step, BinaryRPC is installed on your system and can be found by any other CMake project using `find_package(binaryrpc)`.

### 3. Building and Running the Examples (Optional)

The `examples` directory contains a separate project that demonstrates how to use the installed library.

```bash
# From the root of the binaryrpc repository, navigate to the examples
cd examples

# Create a build directory
cmake -E make_directory build
cd build

# Configure the example project. CMake will find the installed library.
# No toolchain file needed on Linux/macOS.
cmake ..

# Build the examples
cmake --build . --config Release

# Run the basic server example
./basic_server
```

### 4. Using BinaryRPC in Your Own Project

To use BinaryRPC in your own CMake project, simply add the following to your `CMakeLists.txt`:

```cmake
# Find the installed binaryrpc package
find_package(binaryrpc 0.1.0 REQUIRED)

# ...

# Link your executable or library to binaryrpc
# The binaryrpc::core target will automatically bring in all necessary
# dependencies and include directories.
target_link_libraries(your_target_name PRIVATE binaryrpc::core)
```

---

  

## 🚪 Custom handshake & authentication (`IHandshakeInspector`)

  

The **first thing** every WebSocket upgrade hits is an `IHandshakeInspector`. The default one simply accepts the socket and builds a trivial `ClientIdentity` from the URL query‑string. In production you almost always want to **sub‑class** it.

  

Below is a condensed version of the `CustomHandshakeInspector` actually running in the reference **chat server**:

  

```cpp
class CustomHandshakeInspector : public IHandshakeInspector {
public:
    std::optional<ClientIdentity> extract(uWS::HttpRequest& req) override {
        // --- 1. Parse query‑string ------------------------------
        // Expected → ws://host:9010/?clientId=123&deviceId=456&sessionToken=ABCD…

        std::string q{req.getQuery()};
        auto [clientId, deviceIdStr, tokenHex] = parseQuery(q);

        if (clientId.empty() || deviceIdStr.empty())
            return std::nullopt; // missing mandatory IDs

        // --- 2. Validate deviceId is numeric --------------------
        int deviceId;
        try {
            deviceId = std::stoi(deviceIdStr);
        } catch (...) {
            return std::nullopt;
        }

        // --- 3. Check persistent session file ------------------
        if (!sessionFileContains(clientId, deviceIdStr))
            return std::nullopt; // kick unknown combos

        // --- 4. Build / generate sessionToken ------------------
        std::array<std::uint8_t, 16> token{};

        if (tokenHex.size() == 32)
            hexStringToByteArray(tokenHex, token); // reuse supplied token
        else
            token = sha256_16(clientId + ':' + deviceIdStr + ':' + epochMs());

        // --- 5. Emit identity ----------------------------------
        ClientIdentity id;
        id.clientId = clientId;
        id.deviceId = deviceId;
        id.sessionToken = token;

        return id; // non‑nullopt ⇒ upgrade success
    }
};

// Inspector'ı WebSocket sunucusuna ata
ws->setHandshakeInspector(std::make_shared<CustomHandshakeInspector>());

```

  

### Connection Parameters

The sample inspector above expects the client to provide **three** query parameters on the WebSocket URL:
- **`clientId`**: An opaque user string (e.g., email or database ID).
- **`deviceId`**: An integer identifying the physical device.
- **`sessionToken`** (optional): A 32-character hex string to resume a previous session.

If `extract()` returns `std::nullopt`, the upgrade is rejected with a **400 Bad Request**. If the `sessionToken` is absent or invalid, a secure inspector should generate a new one and provide it to the client after a successful connection.

### Alternative: JWT-based Handshake

If you prefer to use standards like JSON Web Tokens (JWT) for authentication, you can write an inspector that checks for an HTTP header instead of query parameters.

```cpp
// Forward declarations for your business logic
ClientIdentity makeClientFromJwt(const std::string& token);
bool verifyJwt(const std::string& token);

class JwtInspector : public IHandshakeInspector {
public:
    std::optional<ClientIdentity> extract(uWS::HttpRequest& req) override {
        // This is illustrative. You would use req.getHeader("x-access-token").
        std::string_view token_sv = req.getHeader("x-access-token");

        if (token_sv.empty()) {
            return std::nullopt;
        }

        std::string token{token_sv};
        if (verifyJwt(token)) {
            return makeClientFromJwt(token);
        }

        return std::nullopt;
    }
};

// In your main setup:
// ws->setHandshakeInspector(std::make_shared<JwtInspector>());
```

### Reusing an active session

  

If the inspector returns a `ClientIdentity` whose `sessionToken` matches a live session (i.e. inside `sessionTtlMs`) with the same `clientId` + `deviceId`, BinaryRPC will:

  

1.  **Attach** the new socket to that session.

2.  **Replay** all QoS‑1/2 frames still waiting in its outbox, so the client sees every offline message.

3. Retain all custom fields you stored via `FrameworkAPI` (e.g. inventories, lobby, XP) – the client continues as if the connection had never dropped.

  

No extra code on your side – just be sure your inspector passes back the *exact* 16‑byte token previously issued.

  

---

  

## 🚦 Reliability & QoS

  

BinaryRPC offers **three delivery tiers** inspired by MQTT but adapted for WebSockets:

  

| Level | Guarantees | Frame flow |
|-------|------------|------------|
| `QoSLevel::None` | *At‑most‑once.* Fire‑and‑forget – lowest latency. | `DATA` → **client** |
| `QoSLevel::AtLeastOnce` | *At‑least‑once.* Server retries until client ACKs. | `DATA` ↔ `ACK` |
| `QoSLevel::ExactlyOnce` | *Exactly‑once.* Two‑phase commit eliminates duplicates. | `PREPARE` ↔ `PREPARE_ACK` ↔ `COMMIT` ↔ `COMPLETE` |

  

### Why `sessionTtlMs` matters 🤔

  

`sessionTtlMs` defines **how long BinaryRPC keeps a disconnected client's session alive in memory and on‑disk outbox**:

  

*  **Seamless reconnects** – mobile/Wi‑Fi users frequently drop for a few seconds. As long as they re‑join within the TTL, the framework stitches the new socket onto the old `Session` so *no one re‑logs or re‑syncs*.

*  **Reliable offline push** – any QoS‑1/2 frames queued meanwhile are flushed the instant the user is back online, **preserving order**.

*  **Duplicate shielding** – ExactlyOnce's commit ledger is also retained, so retries from the old socket are recognised and ignored.

  

Set it small (e.g. 5 s) for kiosk/lan apps to free RAM aggressively; crank it up (minutes) for flaky mobile networks or game lobbies that reconnect on map load.

  

With **ExactlyOnce** BinaryRPC persists outbound frames to the **session outbox**. If the socket drops but the **session** survives (`sessionTtlMs`), any queued frames are auto‑replayed on reconnect – *offline messaging for free.*

  

### `ReliableOptions`

  

Configure granular behaviour via `WebSocketTransport::setReliable(options)`:

  

```cpp

struct  ReliableOptions {

	QoSLevel level = QoSLevel::None; // delivery tier

	std::uint32_t baseRetryMs =  100; // initial retry delay

	std::uint32_t maxBackoffMs =  2'000; // cap for delay

	std::uint16_t maxRetry =  5; // fail after N attempts (0 = infinite)

	std::uint32_t sessionTtlMs =  30'000; // resilience window for reconnect & offline push

	std::shared_ptr<IBackoffStrategy> backoffStrategy; // pluggable curve

};

```

  

### Custom back‑off policy

  

Supply your own `IBackoffStrategy` implementation:

  

```cpp

class  FibonacciBackoff : public  IBackoffStrategy {

	std::chrono::milliseconds  nextDelay(std::size_t  n) const  override {

	if(n <  2) return  {100};

	std::size_t a=0,b=1;

	for(std::size_t i=0;i<n;++i){ 
		std::size_t t=a+b; a=b; b=t;
	}

	return std::chrono::milliseconds(std::min(b*100UL,  5'000UL));

}

};

  

ws->setReliable({

.level = QoSLevel::AtLeastOnce,

.baseRetryMs =  100,

.backoffStrategy = std::make_shared<FibonacciBackoff>()

});

```

  

Your strategy receives the **attempt index** (0‑based) and returns the delay before the next retry. It may be stateless or use RNG for jitter.

  

---

  

## 🗄️ Session management via FrameworkAPI

  

`FrameworkAPI` glues the transport and the lock‑free `SessionManager`.

Use it to **store**, **search** and **push** data to any connected (or recently disconnected!) client.

  

```cpp

using  namespace  binaryrpc;

// helper bound to current runtime

auto& app = App::getInstance();

FrameworkAPI fw{  &app.getSessionManager(),  app.getTransport() };

```

  

### Lifecycle notifications (planned)

  

`SessionManager` currently **does not expose built‑in onCreate/onDestroy callbacks**. If you need presence or audit logging today, implement it via:

  

1.  **Middleware** – add a global middleware; on first RPC from a session record "online", and call `fw.disconnect()` after timeout to record "offline".

2.  **Custom plugin** – poll `app.getSessionManager().allSessions()` every few seconds and diff lists, then emit events.

  

Native hooks are on the roadmap; once merged you'll be able to register lambdas before `app.run()`. Follow issue #42 in the repo for progress.

  

### `indexed` flag – when to flip it

  

| `indexed` | Behaviour | Complexity |
|-----------|-----------|------------|
| `false`  *(default)* | Value kept only in session blob. | Look‑up ⇒ **O(N)** scan. |
| `true` | Value also put into a global hash‑map. | `fw.findBy()` ⇒ **O(1)**. |

  

Use `indexed=true` for identifiers you filter on (e.g. `userId`, `roomId`) and keep transient or high‑cardinality data unindexed.

  

`Session` objects live until you explicitly `disconnect()` them or the transport's `sessionTtlMs` expires. All QoS delivery guarantees survive reconnects within that TTL.

  

### Example: Persist state and target users

  

```cpp

// 1️⃣ Persist state (optionally indexed)

fw.setField(ctx.sessionId(),  "userId", userId, /*indexed=*/true);

fw.setField(ctx.sessionId(),  "username", username); // not indexed

fw.setField(ctx.sessionId(),  "xp",  0);

  

// 2️⃣ Target users later

for (auto& s : fw.findBy("userId", std::to_string(userId))) {

fw.sendToSession(s,  app.getProtocol()->serialize("levelUp", {{"lvl", newLvl}}));

}

```
  

---

  

## 🗺️ Architecture Overview

  ```
+-----------------------------+
| raw bytes --> IProtocol  	  |
|               (parse)       |
+--------------+--------------+
               |
               v
+--------------+--------------+
|        ParsedRequest        |
+--------------+--------------+
               |
               v
+--------------+--------------+
|     MiddlewareChain (*)     |
+--------------+--------------+
               |
               v
+--------------+--------------+
| RpcContext & SessionManager |
+--------------+--------------+
               |
               v
+--------------+--------------+
|           RPCManager        |
+--------------+--------------+
               |
               v
+--------------+--------------+
|    ITransport (send)        |
|         <--> client         |
+-----------------------------+
```
  

*All rectangles are replaceable: implement the interface and plug your own.*

  

---

  

## 🛠️ Customisation Cheat‑Sheet

  

| What you want to change | How |
|-------------------------|-----|
| **QoS level / retries** | `WebSocketTransport::setReliable(ReliableOptions)`. Fields: `level`, `baseRetryMs`, `maxRetry`, `maxBackoffMs`, `sessionTtlMs`, `backoffStrategy`. |
| **Back‑off curve** | Implement `IBackoffStrategy::nextDelay()` and pass via `ReliableOptions.backoffStrategy`. |
| **Serialisation** | Implement `IProtocol` – just parse/serialise and throw `ErrorObj` on bad input. |
| **Transport** | Implement `ITransport` (start/stop/send callbacks). Ready‑made: `WebSocketTransport`. |
| **Middleware** | `using Middleware = std::function<void(RpcContext&, Next)>;` Attach via `App::use` / `useFor`. |
| **Plugins (lifecycle)** | Implement `IPlugin::initialize()`; register with `App::usePlugin`. |
| **Session fields** | `fw.setField(sid, key, value, indexed)` / `fw.getField<T>(sid, key)` – choose `indexed=true` for fast look‑ups |
| **Duplicate RPC guard** | Already built‑in: `qos::DuplicateFilter` – just call `accept(payload, ttl)`; no extra wiring needed. |
| **Logging sink** | `Logger::inst().setSink([](LogLevel l, const std::string& m){ … });` + `setLevel(LogLevel::Debug)`. |

## 🔄 Middleware Management

BinaryRPC provides three ways to attach middleware to your application:

### 1. Global Middleware (`use`)

Attaches middleware to all RPC calls:

```cpp
app.use([](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
    // Runs before every RPC
    LOG_DEBUG("Incoming request: " + method);
    next();
    // Runs after every RPC
});
```

### 2. Single Method Middleware (`useFor`)

Attaches middleware to a specific RPC method:

```cpp
app.useFor("login", [](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
    // Runs only before "login" RPC
    auto req = parseMsgPackPayload(payload);
    if (!validateCredentials(req)) {
        throw ErrorObj("Invalid credentials");
    }
    next();
    // Runs after "login" RPC
});
```

### 3. Multi-Method Middleware (`useForMulti`)

Attaches middleware to multiple RPC methods:

```cpp
app.useForMulti({"send_message", "join_room"}, [](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
    // Runs before "send_message" and "join_room" RPCs
    if (!session.isAuthenticated()) {
        throw ErrorObj("Authentication required");
    }
    next();
    // Runs after these RPCs
});
```

### Middleware Chain Execution

1. Middleware executes in the order they are registered
2. Each middleware must call `next()` to continue the chain
3. If a middleware throws an exception, the chain stops and the error is sent to the client
4. Middleware can modify the `Session` and `payload` before and after the RPC execution

### Common Use Cases

- Authentication/Authorization
- Rate limiting
- Request logging
- Input validation
- Session management
- Error handling

Example of a complete middleware setup:

```cpp
// Global logging
app.use([](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
    LOG_INFO("Request: " + method);
    next();
    LOG_INFO("Response sent");
});

// Auth check for sensitive methods
app.useForMulti({"send_message", "join_room"}, [](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
    if (!session.isAuthenticated()) {
        throw ErrorObj("Please login first");
    }
    next();
});

// Rate limiting for specific method
app.useFor("login", [](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
    if (isRateLimited(session.getClientId())) {
        throw ErrorObj("Too many attempts");
    }
    next();
});
```

### Ready-to-Use Middleware

In addition to writing your own logic, BinaryRPC provides pre-built modules for common scenarios.

#### Rate Limiter (`rate_limiter.hpp`)

Instead of implementing the logic manually as shown in the generic example, you can use the dedicated `RateLimiter` module which uses the Token Bucket algorithm.

**Example:** Limit login attempts to 1 per 5 seconds.
```cpp
#include <binaryrpc/middlewares/rate_limiter.hpp>
#include <binaryrpc/core/app.hpp>

// Configure the RateLimiter: 1 request every 5 seconds (0.2 req/sec).
// The bucket capacity is 1, meaning no bursts are allowed.
auto loginLimiter = binaryrpc::middlewares::RateLimiter(0.2, 1);

// Attach the middleware to the "login" RPC
app.useFor("login", loginLimiter);
```
If a user exceeds the limit, the middleware throws an `ErrorObj`, and the client receives a "Too many requests" message.

#### JWT Authentication (`jwt_auth.hpp`)

This middleware validates incoming RPCs using a JSON Web Token (JWT).

**Example:** Secure the "get_profile" RPC by expecting a token in the payload.
```cpp
#include <binaryrpc/middlewares/jwt_auth.hpp>
#include <binaryrpc/core/app.hpp>
#include <nlohmann/json.hpp>

const std::string jwt_secret = "your-very-secret-key";

// Configure the middleware to extract the token from a "token" field in the payload.
auto authMiddleware = binaryrpc::middlewares::JwtAuth(
    jwt_secret,
    [](const binaryrpc::Payload& payload) -> std::string {
        try {
            auto data = nlohmann::json::from_msgpack(payload);
            return data.value("token", "");
        } catch (...) { return ""; }
    }
);

// Protect methods that require authentication
app.useForMulti({"get_profile", "update_settings"}, authMiddleware);
```

---

  

## 📞 Registering RPCs & Replying

The core of your application is the set of RPC handlers that process client requests. You register them on the `App` instance.

### `registerRPC(method, handler)`

This method binds a handler to a specific method name. The handler is a function that receives the raw payload and an `RpcContext` object. The handler is responsible for parsing the incoming payload and serializing the outgoing payload before replying.

The `IProtocol` interface (`MsgPackProtocol`, `SimpleTextProtocol`, etc.) is responsible for framing the data, not for serializing your application-specific data structures. It wraps your payload (`std::vector<uint8_t>`) with a method name.

Here is a corrected example using `nlohmann/json` to handle the application-level data serialization.

```cpp
// main.cpp
#include "binaryrpc/core/app.hpp"
#include "binaryrpc/core/protocol/msgpack_protocol.hpp"
#include <nlohmann/json.hpp> // You'll need a JSON library
#include <iostream>

int main() {
    auto& app = binaryrpc::App::getInstance();
    
    // Use the MsgPack protocol for framing
    auto protocol = std::make_shared<binaryrpc::MsgPackProtocol>();
    app.setProtocol(protocol);

    // Create a FrameworkAPI instance to interact with sessions
    FrameworkAPI api(&app.getSessionManager(), app.getTransport());

    // Register a "login" handler
    app.registerRPC("login", [&](const std::vector<uint8_t>& payload, binaryrpc::RpcContext& ctx) {
        nlohmann::json response_data;
        try {
            // 1. Parse the incoming payload
            auto request = nlohmann::json::parse(payload);
            std::string username = request.value("username", "");
            std::string password = request.value("password", "");

            // 2. Authenticate the user
            if (username == "admin" && password == "password") {
                std::cout << "Login successful for: " << username << std::endl;
                
                // 3. Store state using FrameworkAPI
                api.setField(ctx.session().id(), "isAuthenticated", true, false);
                api.setField(ctx.session().id(), "username", username, true); // indexed

                // 4. Prepare the response payload
                response_data["status"] = "success";
                response_data["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();

            } else {
                response_data["status"] = "error";
                response_data["reason"] = "Invalid credentials";
            }

        } catch (const std::exception& e) {
            std::cerr << "Error in login handler: " << e.what() << std::endl;
            response_data["status"] = "error";
            response_data["reason"] = "Internal server error";
        }

        // 5. Serialize the response data and reply
        std::string response_str = response_data.dump();
        std::vector<uint8_t> response_bytes(response_str.begin(), response_str.end());
        
        // Use the protocol to frame the response
        ctx.reply(protocol->serialize("login_response", response_bytes));
    });

    // Add an authentication middleware for "get_profile"
    app.useFor("get_profile", [&](Session& session, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
        auto isAuthenticated = api.getField<bool>(session.id(), "isAuthenticated");

        if (!isAuthenticated.value_or(false)) {
            // User is not authenticated. Send an error response directly and stop the chain.
            nlohmann::json err_data = {
                {"status", "error"},
                {"reason", "Authentication required"}
            };
            std::string err_str = err_data.dump();
            std::vector<uint8_t> err_bytes(err_str.begin(), err_str.end());
            
            // Frame the error response
            auto framed_error = protocol->serialize("auth_error", err_bytes);
            
            // Send it directly to the session and DO NOT call next()
            // Note: This creates a shared_ptr copy of the session to pass to the API.
            api.sendToSession(std::make_shared<Session>(session), framed_error);
            return; 
        }
        
        // User is authenticated, proceed to the RPC handler
        next();
    });

    // Register another handler - now protected by middleware
    app.registerRPC("get_profile", [&](const auto& payload, auto& ctx) {
        // Auth check is now in the middleware, we can proceed directly.
        auto username = api.getField<std::string>(ctx.session().id(), "username");
        
        nlohmann::json profile_data;
        profile_data["username"] = username.value_or("N/A");
        // ... other profile data

        std::string profile_str = profile_data.dump();
        std::vector<uint8_t> profile_bytes(profile_str.begin(), profile_str.end());
        
        ctx.reply(protocol->serialize("profile_data", profile_bytes));
    });

    app.run(9010);
    return 0;
}
```

### The `RpcContext`

The `RpcContext` is your gateway to interacting with the client and their session:
- `ctx.reply(payload)`: Sends a message back to the originating client.
- `ctx.broadcast(payload)`: Sends a message to all connected clients.
- `ctx.session()`: Returns a reference to the `Session` object for the current client. You can use this to store and retrieve connection-specific state using `setField` and `getField`.
- `ctx.disconnect()`: Closes the connection.

Delivery of replies respects the QoS settings of the transport, just like `sendToSession()` does (including ACKs, retries, and the offline outbox).

  

---

  

## 🔌 How clients connect

  

### Server‑side expectations

  

The sample inspector above demands **three** query parameters:

  

```

clientId ⇒ opaque user string (e.g. email)

deviceId ⇒ integer; each physical device gets a stable ID

sessionToken (optional) ⇒ 32‑hex chars to resume previous session

```

  

If the token is absent or invalid the server autogenerates a fresh one and returns it via a dedicated `get_token` RPC.

  

### JavaScript reference client

  

The repository ships with a minimal reference client in `index.html`. It demonstrates:

  

```js

import  "./binaryrpc.js";

  

const  rpc  =  new  BinaryRPC("ws://localhost:9010", {

	onConnect() { rpc.call("join"); },

	onError:  console.error

});

  

rpc.connect("user‑42", "web", /*session*/ null);

  

// later

rpc.call("broadcast", { message:  "hello" });

```

  

Internally the helper wraps the QoS‑2 handshake (`DATA → ACK` etc.) and keeps an **outbox** so messages typed while offline flush once the socket is back (mirroring server‑side `sessionTtlMs`). You can plug your own MessagePack/JSON codec by swapping `MessagePack.encode/decode`.

  

### Writing custom handshake/auth logic

  

If you need headers, JWT, IP checks … attach an `IHandshakeInspector`:

  

```cpp
// Forward declarations for your business logic
ClientIdentity makeClientFromJwt(const std::string& token);
bool verifyJwt(const std::string& token);

class JwtInspector : public IHandshakeInspector {
public:
    std::optional<ClientIdentity> extract(uWS::HttpRequest& req) override {
        // Note: uWS::HttpRequest does not have getHeader in this form.
        // This is illustrative. You would use req.getHeader("x-access-token").
        std::string_view token_sv = req.getHeader("x-access-token");

        if (token_sv.empty()) {
            return std::nullopt;
        }

        std::string token{token_sv};
        if (verifyJwt(token)) {
            return makeClientFromJwt(token);
        }

        return std::nullopt;
    }
};

// In your main setup:
// ws->setHandshakeInspector(std::make_shared<JwtInspector>());
```

  

If `extract()` returns `std::nullopt` the upgrade is rejected with **400 Bad Request**.

  

---

  

## 📚 In-Depth Guides & Examples

While the cheat-sheet provides quick answers, this section explains how to use the ready-to-use components that ship with BinaryRPC.

### Plugins

Plugins add complex, stateful capabilities to the framework.

#### Room Plugin

The `room_plugin.hpp` provides core functionality for chat rooms, game lobbies, or any group-based scenario.

**Example:** Joining a room and broadcasting a message.
```cpp
#include <binaryrpc/plugins/room_plugin.hpp>
#include <binaryrpc/core/app.hpp>
#include <binaryrpc/core/framework_api.hpp>

// ... setup app and api ...

// Create and register the RoomPlugin
auto roomPlugin = std::make_unique<binaryrpc::RoomPlugin>(app.getSessionManager(), app.getTransport());
binaryrpc::RoomPlugin* room_ptr = roomPlugin.get();
app.usePlugin(std::move(roomPlugin));

// RPC for a client to join a room
app.registerRPC("join_room", [&](const auto& req, auto& ctx) {
    std::string room_name(req.begin(), req.end()); // e.g., "/gaming"
    room_ptr->join(room_name, ctx.session().id());
    
    // Announce that the user has joined
    std::string join_msg = "User " + ctx.session().identity().clientId + " joined.";
    std::vector<uint8_t> payload(join_msg.begin(), join_msg.end());
    room_ptr->broadcast(room_name, app.getProtocol()->serialize("room_event", payload));
});
```

### Transport Layer

The transport layer is responsible for sending and receiving raw data over the network.

#### WebSocket Transport Callbacks

The `WebSocketTransport` allows you to register a callback that is triggered when a client's connection is terminated or the session expires. This is powerful for tracking user presence or cleaning up resources.

**Example:** Log disconnections and update user status.
```cpp
#include <binaryrpc/transports/websocket/websocket_transport.hpp>
// ... other includes ...

// Create the transport
auto ws = std::make_unique<binaryrpc::WebSocketTransport>(sm, /*idleTimeoutSec=*/60);

// Set the disconnect callback
ws->setDisconnectCallback([&api](std::shared_ptr<binaryrpc::Session> session) {
    if (!session) return;
    
    const auto& identity = session->identity();
    LOG_INFO("Client disconnected: " + identity.clientId);
              
    // Update the user's status to "offline"
    api.setField(session->id(), "onlineStatus", std::string("offline"), /*indexed=*/true);
});

// Set the transport on the app
app.setTransport(std::move(ws));
```

### Core Utilities

The framework provides several utilities in the `binaryrpc/core/util/` directory.

#### Custom Logger

You can redirect the framework's logs to a file or a logging service.

**Example:** Log to both console and a file.
```cpp
#include <binaryrpc/core/util/logger.hpp>
#include <fstream>

std::ofstream logFile("server.log", std::ios::app);

// Set a custom sink
binaryrpc::Logger::inst().setSink([&logFile](binaryrpc::LogLevel lvl, const std::string& msg) {
    static const char* names[]{"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
    std::string log_line = "[" + std::string(names[(int)lvl]) + "] " + msg;
    
    std::cout << log_line << std::endl;
    logFile << log_line << std::endl;
});

binaryrpc::Logger::inst().setLevel(binaryrpc::LogLevel::Debug);
```

#### Custom Error Handling

Throw an `ErrorObj` in your RPC handlers to send a structured error to the client.

**Example:**
```cpp
#include <binaryrpc/core/util/error_types.hpp>

app.registerRPC("place_bet", [&](const auto& req, auto& ctx) {
    if (/* user has insufficient balance */) {
        throw binaryrpc::ErrorObj("Insufficient balance", 1001); // (message, custom_code)
    }
});
```

  
---

  

## 💻 Client-Side Usage (JavaScript)

For a production‑ready browser/Node client, check binaryrpc‑client‑js → https://github.com/efecan0/binaryrpc-client-js.

```js
import  "./binaryrpc.js";

const  rpc  =  new  BinaryRPC("ws://localhost:9010", {
	onConnect() { rpc.call("join"); },
	onError:  console.error
});

// The client expects the server's IHandshakeInspector to understand these params.
rpc.connect("user‑42", "web", /*sessionToken*/ null);

// later
rpc.call("broadcast", { message:  "hello" });
```

The helper wraps the QoS handshake and queues outbound messages while offline. If you prefer another codec, simply replace MessagePack.encode/decode.

---

## 🧪 Running Tests

### 1. Test Dependencies (Catch2)

BinaryRPC's C++ unit tests are written using the [Catch2](https://github.com/catchorg/Catch2) framework. You need to install Catch2 before you can build and run the tests.

- **Windows (vcpkg):**
  ```bash
  ./vcpkg install catch2 --triplet x64-windows
  ```
- **Linux (Arch/pacman):**
  ```bash
  sudo pacman -S catch2
  ```
- **Linux (Ubuntu/Debian):**
  ```bash
  sudo apt install catch2
  ```
> Note: On some systems, Catch2's CMake config files (`Catch2Config.cmake`) may be missing. In that case, you can build and install Catch2 from source by following the instructions on the [Catch2 GitHub page](https://github.com/catchorg/Catch2).

### 2. Building and Running the Tests

By default, the `BINARYRPC_BUILD_TESTS` option in the main CMakeLists.txt is **ON**, so tests are built automatically.

- To build the tests:
  ```bash
  cmake .. -DBINARYRPC_BUILD_TESTS=ON
  cmake --build . --config Release
  ```
- To run all tests:
  ```bash
  ctest --output-on-failure
  ```
- Or you can run the test binaries directly:
  ```bash
  ./tests/binaryrpc_unit_tests
  ./tests/binaryrpc_qos_tests
  ```

### 3. Integration and Memory Leak Tests (Python)

Some integration and memory leak tests are written in Python. To run them:

- Make sure you have Python 3 and the required packages installed. You can install dependencies with:
  ```bash
  pip install -r tests/integration_tests_py/requirements.txt
  ```
- To run the tests, simply execute the scripts directly:
  ```bash
  # Example: Run a memory leak test
  python tests/Memory_leak_stress_test_py/memory_leak_test.py

  # Example: Run an integration test
  python tests/integration_tests_py/session/app.py
  ```
