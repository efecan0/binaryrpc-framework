#pragma once
#include <chrono>
#include <algorithm>                        // std::min
#include "binaryrpc/core/types.hpp"
#include "binaryrpc/core/session/session.hpp"

namespace binaryrpc {

    // Basit Token Bucket
    struct RateBucket {
        int                                   tokens;
        std::chrono::steady_clock::time_point last;
    };

    /**
     * @param qps   : saniyede yenilenen token sayısı
     * @param burst : en fazla birikebilecek token miktarı
     */
    inline auto rateLimiter(int qps, int burst = 2) {
        return [qps, burst](Session& s, NextFunc next) {
            // Session’da saklı Bucket* alalım
            auto bucketPtr = s.get<RateBucket*>("_bucket");
            if (!bucketPtr) {
                bucketPtr = new RateBucket{ burst,
                                            std::chrono::steady_clock::now() };
                s.set<RateBucket*>("_bucket", bucketPtr);
            }
            RateBucket* b = bucketPtr;

            // Zaman farkı kadar token yenile
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - b->last)
                .count();
            if (elapsed > 0) {
                // burst ve int uyumu için cast ediyoruz
                int newTokens = static_cast<int>(b->tokens + elapsed * qps);
                b->tokens = std::min(burst, newTokens);
                b->last = now;
            }

            if (b->tokens > 0) {
                --b->tokens;
                next();
            }
            // token yoksa next() çağrılmaz, istek drop edilir
        };
    }

} // namespace binaryrpc
