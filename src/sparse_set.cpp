#include "ecs/sparse_set.hpp"
#include "ecs/constants.hpp"

ecs::SparseSet::SparseSet()
{
    sparse_.resize(kMaxEntities, kInvalidSparseEntry);

}

bool ecs::SparseSet::contains(Entity to_check) const
{
    if (!to_check.is_valid())
        return false;

    if (!index_of(to_check).has_value()) // check if sparse index is valid
        return false;
    
    std::uint32_t dense_position = sparse_.at(to_check.index);
    if (dense_position >= dense_.size()) // check if position in dense is out of bounds
        return false;

    Entity dense_entity = dense_.at(dense_position);
    if (dense_entity == to_check) // check if the entry matches
        return true;

    return false;
}

bool ecs::SparseSet::insert(Entity to_insert)
{
    if (!to_insert.is_valid())
        return false;

    if (contains(to_insert))
        return false;

    if (to_insert.index >= kMaxEntities)
        return false;

    if (index_of(to_insert).has_value())
        if (dense_[sparse_[to_insert.index]].generation != to_insert.generation)
            return false;

    dense_.push_back(to_insert);
    sparse_[to_insert.index] = dense_.size() - 1;

    return true;
}

bool ecs::SparseSet::erase(Entity to_erase)
{
    if (!to_erase.is_valid())
        return false;

    if (!contains(to_erase))
        return false;
        
    std::uint32_t dense_index_to_erase = sparse_.at(to_erase.index);

    if (dense_index_to_erase == dense_.size() - 1) { // if its the last element we simply remove it
        dense_.pop_back();
    }
    else { // if its not the last we move the last element in its position
        Entity last_in_dense = dense_[dense_.size() - 1];
        dense_[dense_index_to_erase] = last_in_dense;
        dense_.pop_back();
        sparse_[last_in_dense.index] = dense_index_to_erase;
    }

    sparse_[to_erase.index] = kInvalidIndex;
    return true;
}

std::size_t ecs::SparseSet::size() const
{
    return dense_.size();
}

bool ecs::SparseSet::is_empty() const
{
    return dense_.size() == 0;
}

std::optional<std::size_t> ecs::SparseSet::index_of(Entity entity) const
{
    if (entity.index >= sparse_.size()) // check if sparse index is valid
        return std::nullopt;
    if (sparse_[entity.index] == kInvalidSparseEntry)
        return std::nullopt;
    
    return sparse_[entity.index];
}

std::span<const ecs::Entity> ecs::SparseSet::entities() const
{
    return dense_;
}
