#include "ecs/entity_manager.hpp"
#include <iostream>

namespace ecs {
    EntityManager::EntityManager()
    {
        entity_records_.reserve(kMaxEntities);
    }

    std::uint32_t EntityManager::alive_count() const
    {
        return alive_count_;
    }

    bool EntityManager::is_alive(Entity to_check) const
    {   
        if (to_check.index >= entity_records_.size())
            return false;
   
        RecordSlot record = entity_records_.at(to_check.index);
        if (record.generation == to_check.generation)
            return record.is_alive;
        
        return false;
    }

    Entity EntityManager::create_entity()
    {
        Entity new_entity{};

        if (free_indices_.empty()) {
            if (entity_records_.size() == kMaxEntities)
                return new_entity; // max entities reached

            entity_records_.push_back(RecordSlot{kFirstValidGeneration, true});
            alive_count_++;

            new_entity.index = entity_records_.size() - 1;
            new_entity.generation = kFirstValidGeneration;
            return new_entity;
        }

        // take the top from the free indices stack
        new_entity.index = free_indices_.top();
        free_indices_.pop();

        // find the record, update its generation and set it to be alive
        RecordSlot &record = entity_records_.at(new_entity.index);
        record.generation += 1;
        record.is_alive = true;
        alive_count_++;

        new_entity.generation = record.generation;
        return new_entity;
    }
    
    bool EntityManager::destroy_entity(Entity to_destroy)
    {
        // find the record in the records and set it to not alive
        if (is_alive(to_destroy)) {
            RecordSlot &record = entity_records_.at(to_destroy.index);
            if (record.generation != kMaxGeneration) // guard for generation exhaustion
                free_indices_.push(to_destroy.index);
            record.is_alive = false;
            alive_count_--;
            return true;  
        }
        return false;
    }


}
