#include "ecs/registry.hpp"

namespace ecs {

Entity Registry::create_entity()
{
    Entity new_entity = manager_.create_entity();
    return new_entity;
}

bool Registry::destroy_entity(Entity entity)
{
    if (!manager_.is_alive(entity))
        return false;

    for (auto& component_pool : component_pools_) {
        component_pool->erase(entity);
    }
    // make the entity available for reuse as last step
    manager_.destroy_entity(entity);
    
    return true;
}

bool Registry::is_alive(Entity entity) const
{
    return manager_.is_alive(entity);
}
}
