/*
 * cpu_code_cache.cc
 *
 *   Copyright: Copyright 2013 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */

#include "granary/cpu_code_cache.h"

namespace granary {

    enum {
        MAX_SCAN = 8,
        MIN_DEFAULT_ENTRIES = 1024
    };

    /// Find an entry in the CPU-private code cache.
    app_pc cpu_private_code_cache::find(const app_pc key) const throw() {
        if(!entries) {
            return nullptr;
        }

        uint64_t index(reinterpret_cast<uint64_t>(key) & bit_mask);
        for(int m(0); ++m <= MAX_SCAN; index = (index + 1) & bit_mask) {
            cpu_private_code_cache_entry &entry(entries[index]);
            if(!entry.source) {
                break;
            }
            if(key == entry.source) {
                return entry.dest;
            }
        }

        return nullptr;
    }


    /// Insert an entry into the hash table. Returns true iff an element
    /// with the key didn't previously exist in the hash table.
    hash_store_state cpu_private_code_cache::insert(
        app_pc key,
        app_pc value,
        bool update
    ) throw() {
        uint64_t index(reinterpret_cast<uint64_t>(key) & bit_mask);
        unsigned scan(0);
        hash_store_state state(HASH_ENTRY_SKIPPED);

        for(;; ++scan, index = (index + 1) & bit_mask) {
            cpu_private_code_cache_entry &entry(entries[index]);

            // insert position
            if(nullptr == entry.source) {
                entry.dest = value;
                entry.source = key;
                state = HASH_ENTRY_STORED_NEW;
                break;
            }

            // already inserted
            if(entry.source == key) {
                if(update) {
                    entry.dest = value;
                    state = HASH_ENTRY_STORED_OVERWRITE;
                }
                break;
            }
        }

        // resize if we needed to scan too far to insert this element.
        if(MAX_SCAN < scan && !growing) {
            growing = true;
            grow();
            growing = false;
        }

        return state;
    }

    /// Grow the hash table. This increases the hash table's size by two.
    void cpu_private_code_cache::grow(void) throw() {
        cpu_private_code_cache_entry *old_entries(entries);
        const uint64_t old_num_entries(bit_mask + 1);
        const uint64_t new_num_entries(old_num_entries << 1);
        entries = new cpu_private_code_cache_entry[new_num_entries];
        bit_mask = new_num_entries - 1;

        growing = true;
        for(uint64_t i(0); i < old_num_entries; ++i) {
            if(old_entries[i].source) {
                insert(old_entries[i].source, old_entries[i].dest, true);
            }
        }
        growing = false;
        delete old_entries;
    }

    /// Store a value in the hash table. Returns true iff the entry was
    /// written to the hash table.
    bool cpu_private_code_cache::store(
        app_pc key,
        app_pc value,
        hash_store_policy update
    ) throw() {
        if(!entries) {
            entries = new cpu_private_code_cache_entry[MIN_DEFAULT_ENTRIES];
            bit_mask = MIN_DEFAULT_ENTRIES - 1;
        }

        const hash_store_state state(insert(
            key, value, HASH_OVERWRITE_PREV_ENTRY == update));

        if(HASH_ENTRY_STORED_NEW == state) {
            return true;
        }

        return HASH_ENTRY_SKIPPED != state;
    }
}
