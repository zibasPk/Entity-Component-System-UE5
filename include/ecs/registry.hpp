#pragma once
#include "ecs/entity_manager.hpp"
#include "ecs/component_pool_base.hpp"
#include "ecs/component_pool.hpp"
#include "ecs/types.hpp"

#include <vector>
#include <memory>


namespace ecs {
    class Registry {
    public:
        Entity create_entity();
        bool destroy_entity(Entity entity);
        bool is_alive(Entity entity) const;

        template <typename T>
        bool register_component();

        template <typename T>
        ComponentPool<T>* find_pool();

        template <typename T>
        const ComponentPool<T>* find_pool() const;

    private:
        EntityManager manager_;
        std::vector<std::unique_ptr<IComponentPool>> component_pools_;
    };
}
