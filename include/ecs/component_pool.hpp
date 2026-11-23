
#pragma once
#include <ecs/sparse_set.hpp>
#include <ecs/component_pool_base.hpp>
#include <ecs/component_type_id.hpp>
#include <ecs/types.hpp>

#include <type_traits>
#include <vector>
#include <utility>
#include <memory>

namespace ecs {
    template <typename T>
    class ComponentPool : public IComponentPool {
        static_assert(std::is_nothrow_move_constructible_v<T>,
            "Component types must support non-throwing move construction.");
        static_assert(std::is_nothrow_destructible_v<T>,
            "Component types must support non-throwing destruction.");
        static_assert(std::is_nothrow_move_assignable_v<T>,
            "Component types must support non-throwing move assingment move.");
    public:
        template <typename... Args>
        T* try_emplace(Entity entity, Args&&... args);
        T* try_get(Entity entity) noexcept;
        const T* try_get(Entity entity) const noexcept;
        ComponentTypeId type_id() const noexcept override;
        bool erase(Entity entity) noexcept override;
        bool contains(Entity entity) const noexcept override;
        std::size_t size() const noexcept;
        bool is_empty() const noexcept;
    private:
        SparseSet entity_set_; 
        std::vector<T> components_;
    };

    
    template <typename T>
    template <typename... Args>
    inline T *ComponentPool<T>::try_emplace(Entity entity, Args &&...args)
    {
        if(entity_set_.index_of(entity))
            return nullptr;

        components_.emplace_back(std::forward<Args>(args)...);    
        if (!entity_set_.insert(entity)){
            components_.pop_back();
            return nullptr;
        }

        return std::addressof(components_.back());
    }

    template <typename T>
    inline const T *ComponentPool<T>::try_get(Entity entity) const noexcept
    {
        std::optional<std::size_t> dense_index = entity_set_.index_of(entity);
        if (!dense_index.has_value())
            return nullptr;
        
        return std::addressof(components_[dense_index.value()]);
    }

    template <typename T>
    inline ComponentTypeId ComponentPool<T>::type_id() const noexcept
    {   
        return component_type_id<T>();
    }

    template <typename T>
    inline bool ComponentPool<T>::erase(Entity entity) noexcept
    {   
        std::optional<std::size_t> dense_index = entity_set_.index_of(entity);
        if(!dense_index.has_value())
            return false;
        
        if (dense_index.value() == components_.size() - 1) {
            components_.pop_back();
        }
        else {
            T& last_component = components_[components_.size() - 1];
            components_[dense_index.value()] = std::move(last_component);
            components_.pop_back();
        }
        entity_set_.erase(entity);

        return true;
    }

    template <typename T>
    inline T* ComponentPool<T>::try_get(Entity entity) noexcept{
        return const_cast<T*>(
            std::as_const(*this).try_get(entity)
        );
    }


    template <typename T>
    inline bool ComponentPool<T>::contains(Entity entity) const noexcept
    {
        return entity_set_.index_of(entity).has_value();
    }

    template <typename T>
    inline std::size_t ComponentPool<T>::size() const noexcept
    {
        return components_.size();
    }

    template <typename T>
    inline bool ComponentPool<T>::is_empty() const noexcept
    {
        return components_.empty();
    }

 
}
