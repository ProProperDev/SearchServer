#pragma once
#include <mutex>
using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Minmap {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);
 
    struct Access {
        std::lock_guard<std::mutex> lock_;
        Value& ref_to_value;
 
        Access(const Key& key, Minmap& map_)
            : lock_(map_.mutex)
            , ref_to_value(map_.map[key]) {
        }
    };
 
    explicit ConcurrentMap(size_t count_of_minmap)
        : maps_(count_of_minmap) {
    }
 
    Access operator[](const Key& key) {
        auto& map_ = maps_[static_cast<uint64_t>(key) % maps_.size()];
        return {key, map_};
    }
 
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result_map;
        for (auto& [mutex, map] : maps_) {
            std::lock_guard g(mutex);
            result_map.insert(map.begin(), map.end());
        }
        return result_map;
    }
 
private:
    std::vector<Minmap> maps_;
};