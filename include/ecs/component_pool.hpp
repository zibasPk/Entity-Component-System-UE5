
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
            "Component types must support non-throwing move assingment."); 
    public:
        template <typename... Args> T* try_emplace(Entity entity, Args&&... args);
        T* try_get(Entity entity) noexcept;
        const T* try_get(Entity entity) const noexcept;
        ComponentTypeId type_id() const noexcept override;
        /// @brief Erases the given entity from the pool
        /// @param entity 
        /// @return true if removed, false if nothing to remove
        bool erase(Entity entity) noexcept override;
        bool contains(Entity entity) const noexcept override;
        std::size_t size() const noexcept;
        bool is_empty() const noexcept;
        std::span<const Entity> entities() const noexcept override;   // forwards to entity_set_.entities()
        std::span<T> components() noexcept;       // { components_.data(), components_.size() }
        std::span<const T> components() const noexcept;  
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

        if (!entity_set_.insert(entity))
            return nullptr;

        try
        {
            components_.emplace_back(std::forward<Args>(args)...);  
        } 
        catch(...)
        {
            entity_set_.erase(entity);
            throw;
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
    inline T* ComponentPool<T>::try_get(Entity entity) noexcept
    {
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

    template <typename T>
    inline std::span<const Entity> ComponentPool<T>::entities() const noexcept
    {
        return entity_set_.entities();
    }

    template <typename T>
    inline std::span<T> ComponentPool<T>::components() noexcept
    {
        return components_;
    }

    template <typename T>
    inline std::span<const T> ComponentPool<T>::components() const noexcept
    {
        return components_;
    }
}
