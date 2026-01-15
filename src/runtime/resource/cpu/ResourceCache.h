#pragma once
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>

#include "UUID.hpp"

namespace dk {
class ResourceCache
{
public:
    template <class Res>
    std::shared_ptr<Res> resolve(UUID id, std::function<std::shared_ptr<Res>()> loader);

    void invalidate(UUID id); // 文件变动时调用
private:
    std::unordered_map<UUID, std::unordered_map<std::type_index, std::shared_ptr<void>>> _map;
};

template <class Res>
std::shared_ptr<Res> ResourceCache::resolve(UUID id, std::function<std::shared_ptr<Res>()> loader)
{
    auto& perId = _map[id];
    // 尝试在缓存中查找对应uid的资源
    auto  it = perId.find(std::type_index(typeid(Res)));
    if (it != perId.end()) return std::static_pointer_cast<Res>(it->second);

    // 如果没有找到，则调用加载器函数加载资源
    auto res = loader();
    if (res)
        perId[std::type_index(typeid(Res))] = res;
    return res;
}

}
