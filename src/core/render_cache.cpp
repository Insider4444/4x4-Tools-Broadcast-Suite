#include "render_cache.h"
#include <bit>
#include <cstring>

namespace fourbyfour {
void RenderKey::append(const void* data,std::size_t bytes) {
    const auto* p=static_cast<const std::byte*>(data);const auto length=bytes;
    // Two independent word accumulators; this is a cache fingerprint, not a
    // security/integrity primitive. All source pixels participate, not samples.
    while(bytes>=16) {
        std::uint64_t a,b;std::memcpy(&a,p,8);std::memcpy(&b,p+8,8);
        hash[0]=std::rotl((hash[0]^a)*0x9e3779b185ebca87ull,27);
        hash[1]=std::rotl((hash[1]^b)*0xc2b2ae3d27d4eb4full,31);p+=16;bytes-=16;
    }
    while(bytes--){hash[0]=(hash[0]^std::to_integer<unsigned char>(*p++))*1099511628211ull;hash[1]=std::rotl(hash[1]^hash[0],17);}
    hash[0]^=length;hash[1]^=std::rotl(hash[0],23);
}
void RenderKey::append(const Frame& frame) {
    validate(frame);append(&frame.width,sizeof(frame.width));append(&frame.height,sizeof(frame.height));append(&frame.layout,sizeof(frame.layout));
    const auto bytes=frame.width*pixelBytes(frame.layout);
    for(int y=0;y<frame.height;++y)append(static_cast<const std::byte*>(frame.pixels)+y*frame.stride,bytes);
}
bool RenderCache::restore(const RenderKey& key,const Frame& output) {
    validate(output);const auto row=output.width*pixelBytes(output.layout),bytes=row*output.height;
    std::lock_guard lock(mutex);
    for(auto it=entries.begin();it!=entries.end();++it)if(it->key==key&&it->pixels.size()==bytes) {
        for(int y=0;y<output.height;++y)std::memcpy(static_cast<std::byte*>(output.pixels)+y*output.stride,it->pixels.data()+y*row,row);
        entries.splice(entries.begin(),entries,it);return true;
    }return false;
}
void RenderCache::store(const RenderKey& key,const Frame& output) {
    validate(output);const auto row=output.width*pixelBytes(output.layout),bytes=row*output.height;if(bytes>limit)return;
    Entry entry{key,std::vector<std::byte>(bytes)};
    for(int y=0;y<output.height;++y)std::memcpy(entry.pixels.data()+y*row,static_cast<const std::byte*>(output.pixels)+y*output.stride,row);
    std::lock_guard lock(mutex);
    for(auto it=entries.begin();it!=entries.end();)if(it->key==key){used-=it->pixels.size();it=entries.erase(it);}else++it;
    while(!entries.empty()&&used+bytes>limit){used-=entries.back().pixels.size();entries.pop_back();}
    used+=bytes;entries.push_front(std::move(entry));
}
void RenderCache::clear(){std::lock_guard lock(mutex);entries.clear();used=0;}
}
