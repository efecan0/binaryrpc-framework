/**
 * @file generic_index.hpp
 * @brief Generic index for fast field-based session lookup in BinaryRPC.
 *
 * Provides a multi-level index for mapping field values to session IDs, enabling
 * efficient O(1) lookup of sessions by arbitrary fields.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <string>
#include <vector>
#include <mutex>

namespace binaryrpc {
    /**
     * @class GenericIndex
     * @brief Multi-level index for fast field-based session lookup.
     *
     * Maps field names and values to sets of session IDs, enabling efficient lookup
     * and reverse mapping for cleanup.
     */
    class GenericIndex {
    public:
        /**
         * @brief Add a field-value mapping for a session ID.
         * @param sid Session ID
         * @param field Field name
         * @param value Field value
         */
        void add(const std::string& sid,
            const std::string& field,
            const std::string& value);

        /**
         * @brief Remove all index entries for a session ID.
         * @param sid Session ID
         */
        void remove(const std::string& sid);

        /**
         * @brief Find all session IDs matching a field and value.
         * @param field Field name
         * @param value Field value
         * @return Set of matching session IDs
         */
        std::unordered_set<std::string>
            find(const std::string& field,
                const std::string& value) const;

    private:
        using SidSet = std::unordered_set<std::string>;
        std::unordered_map<std::string,                     // field
            std::unordered_map<std::string, SidSet>> idx_;  // value → {sid…}

        /**
         * @brief Reverse mapping for cleanup: session ID to list of field-value pairs.
         */
        std::unordered_map<std::string,
            std::vector<std::pair<std::string, std::string>>> back_;

        mutable std::shared_mutex mx_;
    };

}