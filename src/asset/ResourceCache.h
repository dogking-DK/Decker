#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include "../core/UUID.hpp"

namespace dk {
class ResourceCache
{
public:
    template <class Res>
    std::shared_ptr<Res> resolve(UUID id, std::function<std::shared_ptr<Res>()> loader);

    void invalidate(UUID id); // �ļ��䶯ʱ����
private:
    std::unordered_map<UUID, std::unordered_map<std::type_index, std::shared_ptr<void>>> _map;
};

template <class Res>
std::shared_ptr<Res> ResourceCache::resolve(UUID id, std::function<std::shared_ptr<Res>()> loader)
{
    auto& perId = _map[id];
    auto  it = perId.find(std::type_index(typeid(Res)));
    if (it != perId.end()) return std::static_pointer_cast<Res>(it->second);

    auto res = loader();
    perId[std::type_index(typeid(Res))] = res;
    return res;
}

}
