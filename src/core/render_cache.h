#pragma once
#include "frame.h"
#include <array>
#include <cstddef>
#include <list>
#include <mutex>
#include <span>

namespace fourbyfour {
class RenderKey {
    std::array<std::uint64_t,2> hash{0x6a09e667f3bcc909ull,0xbb67ae8584caa73bull};
public:
    void append(const void* data,std::size_t bytes);
    void append(const Frame& frame);
    bool operator==(const RenderKey&) const=default;
};
// CPU results only. No GPU models or user media are persisted to disk.
class RenderCache {
    struct Entry {RenderKey key;std::vector<std::byte> pixels;};
    std::list<Entry> entries;
    std::size_t used{},limit;
    std::mutex mutex;
public:
    explicit RenderCache(std::size_t bytes=256ULL*1024*1024):limit(bytes){}
    bool restore(const RenderKey&,const Frame& output);
    void store(const RenderKey&,const Frame& output);
    void clear();
};
}
