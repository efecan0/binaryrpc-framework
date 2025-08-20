// Core API - everything you always need
#include "binaryrpc/binaryrpc.hpp"

// Optional includes for specific implementations
#include <uwebsockets/App.h>
#include "binaryrpc/transports/websocket/websocket_transport.hpp"
#include "binaryrpc/core/strategies/linear_backoff.hpp"
#include "binaryrpc/plugins/room_plugin.hpp"
#include "binaryrpc/core/protocol/msgpack_protocol.hpp"
#include "binaryrpc/middlewares/rate_limiter.hpp"
#include "binaryrpc/core/util/DefaultInspector.hpp"

// Other standard libraries
#include <openssl/sha.h>
#include <chrono>
#include <format>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "utils/parser.hpp"

using namespace binaryrpc;

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

int main() {
    Logger::inst().setLevel(LogLevel::Debug);

    App& app = App::getInstance();
    SessionManager& sm = app.getSessionManager();

    auto ws = std::make_unique<WebSocketTransport>(
        sm,
        /*idleTimeoutSec=*/30,
        /*maxPayloadBytes=*/16 * 1024 * 1024);

    ReliableOptions opts;
        opts.level = QoSLevel::ExactlyOnce;
        opts.baseRetryMs = 1000;
        opts.maxRetry = 8;
        opts.maxBackoffMs = 10000;
        opts.sessionTtlMs = 360'000;
        opts.backoffStrategy = std::make_shared<LinearBackoff>(
            std::chrono::milliseconds(opts.baseRetryMs),
            std::chrono::milliseconds(opts.maxBackoffMs)
        );

    ws->setReliable(opts);
    
    class CustomHandshakeInspector : public IHandshakeInspector {
        public:
            std::optional<ClientIdentity> extract(uWS::HttpRequest& req) override {
                std::string query{req.getQuery()};
                std::string clientId, deviceId, sessionToken;

                if (!query.empty()) {
                    size_t pos = 0;
                    while ((pos = query.find('&')) != std::string::npos) {
                        std::string pair = query.substr(0, pos);
                        query.erase(0, pos + 1);
                        
                        size_t eqPos = pair.find('=');
                        if (eqPos != std::string::npos) {
                            std::string key = pair.substr(0, eqPos);
                            std::string value = pair.substr(eqPos + 1);

                            key = urlDecode(key);
                            value = urlDecode(value);
                            
                            if (key == "clientId") clientId = value;
                            else if (key == "deviceId") deviceId = value;
                            else if (key == "sessionToken") sessionToken = value;
                        }
                    }
                    size_t eqPos = query.find('=');
                    if (eqPos != std::string::npos) {
                        std::string key = query.substr(0, eqPos);
                        std::string value = query.substr(eqPos + 1);

                        key = urlDecode(key);
                        value = urlDecode(value);
                        
                        if (key == "clientId") clientId = value;
                        else if (key == "deviceId") deviceId = value;
                        else if (key == "sessionToken") sessionToken = value;
                    }
                }

                if (clientId.empty() || deviceId.empty()) {
                    LOG_ERROR("Missing required parameters: clientId=" + clientId + ", deviceId=" + deviceId);
                    return std::nullopt;
                }

                try {
                    std::stoi(deviceId);
                } catch (...) {
                    LOG_ERROR("Invalid device ID format: " + deviceId);
                    return std::nullopt;
                }

                if (sessionToken.empty() || sessionToken.size() != 32) {
                    sessionToken = generateSessionToken(clientId, deviceId);
                } else {
                    std::string hexToken = sessionToken;
                    sessionToken.clear();
                    for (size_t i = 0; i < hexToken.length(); i += 2) {
                        std::string byteString = hexToken.substr(i, 2);
                        char byte = static_cast<char>(std::stoi(byteString, nullptr, 16));
                        sessionToken += byte;
                    }
                }

                ClientIdentity identity;
                identity.clientId = clientId;
                identity.deviceId = std::stoi(deviceId);
                
                std::array<std::uint8_t, 16> tokenArray{};
                for (size_t i = 0; i < 16 && i < sessionToken.size(); ++i) {
                    tokenArray[i] = static_cast<std::uint8_t>(sessionToken[i]);
                }
                identity.sessionToken = tokenArray;
                
                return identity;
            }

        private:

            std::string generateSessionToken(const std::string& clientId, const std::string& deviceId) {
                auto now = std::chrono::system_clock::now();
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()
                ).count();
                
                std::string raw = clientId + ":" + deviceId + ":" + std::to_string(now_ms);
                
                unsigned char hash[SHA256_DIGEST_LENGTH];
                SHA256_CTX sha256;
                SHA256_Init(&sha256);
                SHA256_Update(&sha256, raw.c_str(), raw.size());
                SHA256_Final(hash, &sha256);
                
                std::stringstream ss;
                for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
                }
                
                return ss.str();
            }

            std::string urlDecode(const std::string& str) {
                std::string result;
                for (size_t i = 0; i < str.length(); ++i) {
                    if (str[i] == '%' && i + 2 < str.length()) {
                        int value;
                        std::istringstream iss(str.substr(i + 1, 2));
                        iss >> std::hex >> value;
                        result += static_cast<char>(value);
                        i += 2;
                    } else {
                        result += str[i];
                    }
                }
                return result;
           }
        };





    ws->setHandshakeInspector(std::make_shared<CustomHandshakeInspector>());
    app.setProtocol(std::make_unique<MsgPackProtocol>());
    app.setTransport(std::move(ws));


    FrameworkAPI api(&sm, app.getTransport());

    auto roomPlugin = std::make_unique<RoomPlugin>(app.getSessionManager(), app.getTransport());
    RoomPlugin* roomPtr = roomPlugin.get();
    app.usePlugin(std::move(roomPlugin));

    app.registerRPC("join", [&](auto const& req, RpcContext& ctx) {
        auto payload = parseMsgPackPayload(req);
        std::string user = payload["username"].template get<std::string>();
        std::string roomname = payload["roomname"].template get<std::string>();

        api.setField(ctx.session().id(), "username", user, /*indexed=*/true);
        api.setField(ctx.session().id(), "activeRoom", roomname, /*indexed=*/true);
        roomPtr->join(roomname, ctx.session().id());
        nlohmann::json message;
        message["code"] = "JOIN";
        message["message"] = "You have joined the room";
        std::string jsonStr = message.dump();   
        std::vector<uint8_t> outgoingBytes(jsonStr.begin(), jsonStr.end());
        ctx.reply(app.getProtocol()->serialize("message" ,outgoingBytes));
        });

    app.useForMulti({"say","leave"}, [&](Session& s, const std::string& method, std::vector<uint8_t>& req, std::function<void()> next) {
            auto optRoom = api.getField<std::string>(s.id(), "activeRoom");
            if (optRoom) {
                next();
                return;
            }
            nlohmann::json error;
            error["code"] = "ERROR";
            error["message"] = "You are not authorized to perform this action"; 
            std::string jsonStr = error.dump();   
            std::vector<uint8_t> outgoingBytes(jsonStr.begin(), jsonStr.end());
            api.sendTo(s.id(), app.getProtocol()->serialize("message", outgoingBytes));
        });

    app.registerRPC("say", [&](auto const& req, RpcContext& ctx) {
        auto payload = parseMsgPackPayload(req);
        std::string sayMessage = payload["message"].template get<std::string>();
        auto optRoom = api.getField<std::string>(ctx.session().id(), "activeRoom");
        auto optUserName = api.getField<std::string>(ctx.session().id(), "username");
        nlohmann::json message;
        message["code"] = "MESSAGE";
        message["message"] = sayMessage;
        message["username"] = optUserName.value_or("");
        std::string jsonStr = message.dump();   
        std::vector<uint8_t> outgoingBytes(jsonStr.begin(), jsonStr.end());
        
        std::vector<std::string> roomMembers = roomPtr->getRoomMembers(*optRoom);
        for (const auto& member : roomMembers) {
            api.sendTo(member, app.getProtocol()->serialize("message", outgoingBytes));
        }

        });

    app.registerRPC("leave", [&](auto const&, RpcContext& ctx) {
        auto optRoom = api.getField<std::string>(ctx.session().id(), "activeRoom");
        roomPtr->leave(*optRoom, ctx.session().id());
        api.setField(ctx.session().id(), "activeRoom", std::string(), /*indexed=*/false);
        nlohmann::json message;
        message["code"] = "LEAVE";
        message["message"] = "You have left the room";
        std::string jsonStr = message.dump();   
        std::vector<uint8_t> outgoingBytes(jsonStr.begin(), jsonStr.end());
        ctx.reply(app.getProtocol()->serialize("message", outgoingBytes));
        });

    app.registerRPC("get_token", [&api, &app](const std::vector<uint8_t>& /*req*/, RpcContext& ctx) {
            const auto& session = ctx.session();
            const auto& identity = session.identity();

            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (const auto& byte : identity.sessionToken) {
                ss << std::setw(2) << static_cast<int>(byte);
            }
            std::string token = ss.str();

            nlohmann::json response = {
                {"token", token}
            };
            std::string jsonStr = response.dump();
            std::vector<uint8_t> payload(jsonStr.begin(), jsonStr.end());

            ctx.reply(app.getProtocol()->serialize("get_token", payload));
        });

    app.getTransport()->setDisconnectCallback([&](std::shared_ptr<Session> s) {
        //roomPtr->leaveAll(s->id());
        LOG_INFO("User " + s->id() + " disconnected");
        });

    constexpr uint16_t PORT = 5555;
    std::cout << "[Server] ws://localhost:" << PORT << "\n";
    app.run(PORT);

    std::cin.get();
    return 0;
}
