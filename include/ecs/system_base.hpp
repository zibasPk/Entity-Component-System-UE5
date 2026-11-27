#pragma once

#include <ecs/registry.hpp>

namespace ecs{
    class ISystem {
    public:
        virtual ~ISystem() = default;
        virtual void update(Registry& registry, float delta_seconds) = 0;
    };
}
