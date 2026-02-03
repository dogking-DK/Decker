#pragma once

#include <vector>

#include "input/InputContext.h"
#include "input/InputState.h"

namespace dk::input {

class IInputConsumer
{
public:
    virtual ~IInputConsumer() = default;
    virtual bool handleInput(const InputState& state, const InputContext& ctx) = 0;
};

class InputRouter
{
public:
    void addConsumer(IInputConsumer* consumer, int priority = 0);
    void removeConsumer(IInputConsumer* consumer);

    void route(const InputState& state, const InputContext& ctx);

private:
    struct Entry
    {
        int            priority{0};
        IInputConsumer* consumer{nullptr};
    };

    std::vector<Entry> _entries;
};

} // namespace dk::input
