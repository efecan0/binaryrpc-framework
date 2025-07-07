#include "binaryrpc/core/session/generic_index.hpp"

namespace binaryrpc {

    /*──────────── add / update ───────────*/
    void GenericIndex::add(const std::string& sid,
        const std::string& field,
        const std::string& value)
    {
        std::unique_lock w(mx_);

        auto& hist = back_[sid];
        for (auto& fv : hist) {
            if (fv.first == field) {          // same field
                if (fv.second == value)       // value hasn't changed → no-op
                    return;

                // remove the old mapping
                if (auto fit = idx_.find(field); fit != idx_.end())
                    fit->second[fv.second].erase(sid);

                fv.second = value;            // store the new value in history
                idx_[field][value].insert(sid);
                return;
            }
        }

        // 2) initial insertion
        idx_[field][value].insert(sid);
        hist.emplace_back(field, value);
    }

    /*──────────── remove session ─────────*/
    void GenericIndex::remove(const std::string& sid) {
        std::unique_lock w(mx_);
        auto it = back_.find(sid);
        if (it == back_.end()) return;

        for (auto& [f, v] : it->second) {
            auto fit = idx_.find(f);
            if (fit == idx_.end()) continue;

            auto& valueMap = fit->second;
            auto vit = valueMap.find(v);
            if (vit == valueMap.end()) continue;

            vit->second.erase(sid);
            if (vit->second.empty())
                valueMap.erase(v);

            if (valueMap.empty())
                idx_.erase(f);
        }

        back_.erase(it);
    }


    /*──────────── O(1) lookup ────────────*/
    std::unordered_set<std::string>
        GenericIndex::find(const std::string& field,
            const std::string& value) const
    {
        std::shared_lock r(mx_);
        auto fit = idx_.find(field);
        if (fit == idx_.end()) return {};
        auto vit = fit->second.find(value);
        if (vit == fit->second.end()) return {};
        return vit->second;
    }

}
