#pragma once
#include <ecs/types.hpp>

namespace ecs {

    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;
        virtual bool erase(Entity entity) = 0;
        virtual bool contains(Entity entity) const = 0;
    };
}