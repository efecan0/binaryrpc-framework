#pragma once
#include <string>
#include <functional>
#include "binaryrpc/core/middleware/middleware_chain.hpp"
#include "binaryrpc/core/session/session.hpp"

#include <jwt-cpp/jwt.h>

namespace binaryrpc {

    /**
     * JWT tabanlı kimlik doğrulama middleware’i.
     *
     * @param secret        : HS256 imza anahtarı
     * @param requiredRole  : boş değilse payload içindeki "role" claim’inin bu string olmasını şart koşar
     */
    inline auto jwtAuth(const std::string& secret,
        const std::string& requiredRole = "") {
        return [secret, requiredRole](Session& s, NextFunc next) {
            try {
                // İstemci Login sırasında Session’a koymuş olmalı
                auto token = s.get<std::string>("jwt");
                if (token.empty()) return;  // token yoksa reddet

                // Decode & imza doğrulama
                auto decoded = jwt::decode(token);
                jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{ secret })
                    .verify(decoded);

                // Rol kontrolü
                auto roleClaim = decoded.get_payload_claim("role");
                std::string role = roleClaim.as_string();
                if (!requiredRole.empty() && role != requiredRole) {
                    return; // rol yetkisi yok
                }

                // Başarılı → role bilgisini Session’a set edelim
                s.set<std::string>("role", role);
                next();
            }
            catch (...) {
                // herhangi bir hata (parse, verify, claim eksikliği) → istek reddedilsin
            }
        };
    }

} // namespace binaryrpc
