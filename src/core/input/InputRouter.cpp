#include "input/InputRouter.h"

#include <algorithm>

namespace dk::input {

void InputRouter::addConsumer(IInputConsumer* consumer, int priority)
{
    _entries.push_back({priority, consumer});
    std::sort(_entries.begin(), _entries.end(),
              [](const Entry& a, const Entry& b) { return a.priority > b.priority; });
}

void InputRouter::removeConsumer(IInputConsumer* consumer)
{
    _entries.erase(std::remove_if(_entries.begin(), _entries.end(),
                                  [consumer](const Entry& entry) { return entry.consumer == consumer; }),
                   _entries.end());
}

void InputRouter::route(const InputState& state, const InputContext& ctx)
{
    for (const auto& entry : _entries)
    {
        if (!entry.consumer)
        {
            continue;
        }
        if (entry.consumer->handleInput(state, ctx))
        {
            break;
        }
    }
}

} // namespace dk::input
