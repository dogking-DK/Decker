// runtime/ResourceCache.cpp
#include "ResourceCache.h"

namespace dk {
void ResourceCache::invalidate(UUID id) { _map.erase(id); }


// template needs header definition
}
