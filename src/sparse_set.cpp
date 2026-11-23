#include "ecs/sparse_set.hpp"
#include "ecs/constants.hpp"
#include <utility>


ecs::SparseSet::SparseSet()
{
    sparse_.resize(kMaxEntities, kInvalidSparseEntry);

}

bool ecs::SparseSet::insert(Entity to_insert)
{
    if (!to_insert.is_valid()) // entity handle validity
        return false;

    if (to_insert.index >= kMaxEntities) // out of possible index bounds
        return false;

    if (is_valid_sparse(to_insert.index)) // check if there already is a active index (indipendetly from the generation)
        return false;
    
    dense_.push_back(to_insert);
    sparse_[to_insert.index] = static_cast<std::uint32_t>(dense_.size() - 1);

    return true;
}

bool ecs::SparseSet::erase(Entity to_erase) noexcept
{
    if (!to_erase.is_valid())
        return false;

    if (!index_of(to_erase).has_value())
        return false;
        
    std::uint32_t dense_index_to_erase = sparse_[to_erase.index];

    if (dense_index_to_erase == dense_.size() - 1) { // if its the last element we simply remove it
        dense_.pop_back();
    }
    else { // if its not the last we move the last element in its position
        Entity& last_in_dense = dense_[dense_.size() - 1];
        dense_[dense_index_to_erase] = std::move(last_in_dense);
        auto last_in_dense_idx = last_in_dense.index;
        dense_.pop_back();
        sparse_[last_in_dense_idx] = dense_index_to_erase;
    }

    sparse_[to_erase.index] = kInvalidIndex;
    return true;
}

std::size_t ecs::SparseSet::size() const noexcept
{
    return dense_.size();
}

bool ecs::SparseSet::is_empty() const noexcept
{
    return dense_.size() == 0;
}

bool ecs::SparseSet::is_valid_sparse(std::uint32_t index) const noexcept
{
    if (index >= sparse_.size()) // check if sparse bounds
        return false;

    std::uint32_t dense_index = sparse_[index];
    
    if (dense_index == kInvalidSparseEntry || dense_index >= dense_.size())
        return false;

    return true;
}

std::optional<std::size_t> ecs::SparseSet::index_of(Entity entity) const noexcept
{
    if (!is_valid_sparse(entity.index)) 
        return std::nullopt;

    std::uint32_t dense_index = sparse_[entity.index];
    if (dense_[dense_index] != entity)
        return std::nullopt; 
    
    return sparse_[entity.index];
}

std::span<const ecs::Entity> ecs::SparseSet::entities() const noexcept
{
    return dense_;
}
