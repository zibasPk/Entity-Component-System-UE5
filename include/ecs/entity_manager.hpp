#pragma once
#include <vector>
#include <stack>
#include <cstdint>
#include "types.hpp"
#include "constants.hpp"

namespace ecs {
    struct RecordSlot {
        Generation generation;
        bool is_alive;
    };

    class EntityManager {
        public:

            //todo: implement copy and move operators

            // disables copy and move
            EntityManager(const EntityManager&) = delete;
            EntityManager& operator=(const EntityManager&) = delete;
            EntityManager(EntityManager&&) = delete;
            EntityManager& operator=(EntityManager&&) = delete;

            EntityManager();

            std::uint32_t alive_count() const;
            bool is_alive(Entity to_check) const;
            Entity create_entity();
            bool destroy_entity(Entity to_destroy);
        
        private:
            std::uint32_t alive_count_ = 0;
            std::vector<RecordSlot> entity_records_;
            std::stack<EntityIndex> free_indices_;
    };
}
