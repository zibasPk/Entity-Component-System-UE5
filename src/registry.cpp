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

    for (auto& record : component_pools_) {
        record.pool->erase(entity);
    }
    // make the entity available for reuse as last step
    manager_.destroy_entity(entity);
    
    return true;
}

bool Registry::is_alive(Entity entity) const noexcept
{
    return manager_.is_alive(entity);
}

std::optional<ComponentIndex> Registry::find_index(ComponentTypeId id) const noexcept
{
    std::optional<ComponentIndex> index = std::nullopt;
    for (std::size_t i = 0; i < component_pools_.size(); ++i) {
        auto& record = component_pools_[i];
        if (record.type_id == id){
            index = static_cast<ComponentIndex>(i); 
            break;
        }
    }
    return index;
}
}
