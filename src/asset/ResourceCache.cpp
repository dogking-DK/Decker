// runtime/ResourceCache.cpp
#include "ResourceCache.h"

namespace dk {
void ResourceCache::invalidate(UUID id) { _map.erase(id); }

template <class Res>
std::shared_ptr<Res> ResourceCache::resolve(UUID id, std::function<std::shared_ptr<Res>()> loader)
{
    auto& perId = _map[id];
    auto  it    = perId.find(std::type_index(typeid(Res)));
    if (it != perId.end()) return std::static_pointer_cast<Res>(it->second);

    auto res                            = loader();
    perId[std::type_index(typeid(Res))] = res;
    return res;
}
// template needs header definition
}
