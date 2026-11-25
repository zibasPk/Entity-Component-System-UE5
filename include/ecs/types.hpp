#pragma once
#include <cstdint>
#include <limits>

namespace ecs{ 

    using ComponentTypeId = std::uint64_t; // stable, name-derived hash
    using ComponentIndex  = std::uint32_t; // slot in the registrys' component pools collection, < kMaxComponents
    using EntityIndex = std::uint32_t;
    using Generation = std::uint32_t;

    constexpr EntityIndex kInvalidIndex = std::numeric_limits<std::uint32_t>::max();
    constexpr Generation kInvalidGeneration = 0;
    constexpr Generation kMaxGeneration = std::numeric_limits<std::uint32_t>::max();

    constexpr Generation kFirstValidGeneration = 1;

    struct Entity {
        EntityIndex index = kInvalidIndex;
        Generation generation = kInvalidGeneration;
        
        bool operator==(const Entity& other) const noexcept {
            return index == other.index && generation == other.generation;
        };

        bool is_valid() const noexcept {
            return index != kInvalidIndex && generation != kInvalidGeneration;
        }
    };

}
