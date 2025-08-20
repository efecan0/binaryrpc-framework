#include "internal/core/session/generic_index.hpp"

namespace binaryrpc {

    /*──────────── add / update ───────────*/
    void GenericIndex::add(const std::string& sid,
        const std::string& field,
        const std::string& value)
    {
        std::unique_lock w(mx_);

        // 1. Önceki değeri (varsa) kaldır
        auto& hist = back_[sid];
        for (auto& fv : hist) {
            if (fv.first == field) {
                if (fv.second == value) return; // Değer aynı, işlem yapma

                // Eski girdiyi idx_'den sil
                if (auto fit = idx_.find(field); fit != idx_.end()) {
                    if (auto vit = fit->second.find(fv.second); vit != fit->second.end()) {
                        vit->second->erase(sid);
                        if (vit->second->empty()) {
                            fit->second.erase(vit);
                        }
                    }
                }
                fv.second = value; // Geçmişi yeni değerle güncelle
            }
        }

        // 2. Yeni değeri ekle
        auto& valueMap = idx_[field];
        auto it = valueMap.find(value);
        if (it == valueMap.end()) {
            it = valueMap.emplace(value, std::make_shared<SidSet>()).first;
        }
        it->second->insert(sid);

        // 3. Geçmişe ekle (eğer yeni bir alan ise)
        bool fieldExists = false;
        for (const auto& fv : hist) {
            if (fv.first == field) {
                fieldExists = true;
                break;
            }
        }
        if (!fieldExists) {
            hist.emplace_back(field, value);
        }
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

            if(vit->second) {
                vit->second->erase(sid);
                if (vit->second->empty())
                    valueMap.erase(vit);
            }

            if (valueMap.empty())
                idx_.erase(fit);
        }

        back_.erase(it);
    }


    /*──────────── O(1) lookup ────────────*/
    std::shared_ptr<const std::unordered_set<std::string>>
        GenericIndex::find(const std::string& field,
            const std::string& value) const
    {
        std::shared_lock r(mx_);
        auto fit = idx_.find(field);
        if (fit == idx_.end()) return nullptr;
        auto vit = fit->second.find(value);
        if (vit == fit->second.end()) return nullptr;
        return vit->second;
    }

}
