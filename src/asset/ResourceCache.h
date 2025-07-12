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

    void invalidate(UUID id); // 文件变动时调用
private:
    std::unordered_map<UUID, std::unordered_map<std::type_index, std::shared_ptr<void>>> _map;
};
}
