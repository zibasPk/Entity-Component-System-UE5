#pragma once
#include <ecs/types.hpp>
#include <span>

namespace ecs {

    class IComponentPool {
    public:
        // disables copy and move
        IComponentPool(const IComponentPool&) = delete;
        IComponentPool& operator=(const IComponentPool&) = delete;
        IComponentPool(IComponentPool&&) = delete;
        IComponentPool& operator=(IComponentPool&&) = delete;

        virtual ~IComponentPool() = default;
        virtual ComponentTypeId type_id() const noexcept = 0;
        virtual bool erase(Entity entity) noexcept = 0;
        virtual bool contains(Entity entity) const noexcept = 0;
        virtual std::span<const Entity> entities() const noexcept = 0;

    protected:
        IComponentPool() = default;
    };
}