#pragma once
#include <vector>
#include <limits>
#include <ecs/types.hpp>
#include <optional>
#include <span>

namespace ecs {

    class SparseSet {

    public:
        //todo: implement copy and move operators

        // disables copy and move
        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;
        SparseSet(SparseSet&&) = delete;
        SparseSet& operator=(SparseSet&&) = delete;


        SparseSet();
        bool contains(Entity to_check) const;
        bool insert(Entity to_insert);
        bool erase(Entity to_erase);
        std::size_t size() const;
        bool is_empty() const;
        std::optional<std::size_t> index_of(Entity entity) const;
        std::span<const Entity> entities() const;

    private:
        static constexpr std::uint32_t kInvalidSparseEntry = std::numeric_limits<std::uint32_t>::max();
        std::vector<std::uint32_t> sparse_; //index to position in dense array
        std::vector<Entity> dense_;

    };

}