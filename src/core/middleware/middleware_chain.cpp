#include "binaryrpc/core/middleware/middleware_chain.hpp"
#include "binaryrpc/core/util/logger.hpp"  // logger include

namespace binaryrpc {

void MiddlewareChain::add(Middleware mw) { global_.push_back(std::move(mw)); }

void MiddlewareChain::addFor(const std::string& m, Middleware mw) {
    scoped_[m].push_back(std::move(mw));
}

bool MiddlewareChain::execute(Session& s, const std::string& m, std::vector<uint8_t>& payload) {
    std::vector<Middleware> chain = global_;
    if (auto it = scoped_.find(m); it != scoped_.end())
        chain.insert(chain.end(), it->second.begin(), it->second.end());

    size_t idx = 0;

    std::function<void()> next = [&]() {
        if (idx >= chain.size()) return;   // end of the chain
        size_t current = idx;              // current middleware
        bool nextCalled = false;

        ++idx;  // move to next index

        try {
            chain[current](s, m, payload, [&] {
                nextCalled = true;
                next();  // proceed to the next middleware
            });
        } catch (const std::exception& ex) {
            LOG_ERROR("[MiddlewareChain] Exception caught: " + std::string(ex.what()));
            idx = chain.size() + 1;  // stop the chain
        } catch (...) {
            LOG_ERROR("[MiddlewareChain] Unknown exception caught!");
            idx = chain.size() + 1;  // stop the chain
        }

        if (!nextCalled) {
            LOG_WARN("[MiddlewareChain] next() çağrılmadı → zincir durdu.");
            idx = chain.size() + 1;  // stop the chain
        }
    };

    if (!chain.empty())
        next();

    return idx == chain.size();  //return true if the whole chain was executed
}

}
