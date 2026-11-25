#pragma once
#include "ecs/entity_manager.hpp"
#include "ecs/component_pool_base.hpp"
#include "ecs/component_pool.hpp"
#include "ecs/component_type_id.hpp"
#include "ecs/types.hpp"
#include "ecs/constants.hpp"

#include <vector>
#include <memory>
#include <optional>
#include <cassert>


namespace ecs {
    class Registry {
    public:
        Entity create_entity();
        bool destroy_entity(Entity entity);
        bool is_alive(Entity entity) const noexcept;

        /// @brief Adds an entry in component_pools_ for component T if not already present or full
        /// @tparam T 
        /// @return Index of component pool of type T in the component_pools_ vector
        template <typename T> std::optional<ComponentIndex> register_component();

        /// @brief Finds the position of the given component pool in the component_pools_ vector
        /// @tparam T 
        /// @return the position of the component type in the component_pools_ vector, nullopt if not found
        template <typename T> std::optional<ComponentIndex> find_index() const noexcept;

        /// @brief Finds the position of the component pool with the given type id.
        /// @param id type id of the searched component pool
        /// @return the position of the component type in the component_pools_ vector, nullopt if not found
        std::optional<ComponentIndex> find_index(ComponentTypeId id) const noexcept;

        template <typename T> const ComponentPool<T>* find_pool() const noexcept;

        template <typename T> ComponentPool<T>* find_pool() noexcept;

        template <typename T, typename... Args> T* add_component(Entity entity, Args&&... args);
        template <typename T> T* get_component(Entity entity) noexcept;
        template <typename T> const T* get_component(Entity entity) const noexcept;
        /// @brief Removes a component from an entity
        /// @tparam T 
        /// @param entity 
        /// @return true if removed, false if nothing to remove
        template <typename T> bool remove_component(Entity entity) noexcept;
        template <typename T> bool has_component(Entity entity) const noexcept;
        /// @brief Iterate over all entities who posses a certain component.
        /// @tparam T 
        /// @tparam Fn 
        /// @param fn 
        template <typename T, typename Fn> void for_each(Fn&& fn);

        template <typename... Ts> class View;                    // or a free class taking pool pointers
        template <typename... Ts> View<Ts...> view() noexcept;

    private:
        struct PoolRecord {
            ComponentTypeId type_id;
            std::unique_ptr<IComponentPool> pool;
        };
        EntityManager manager_;
        std::vector<PoolRecord> component_pools_;

    };

    template <typename T>
    inline std::optional<ComponentIndex> Registry::register_component()
    {   
        std::optional<ComponentIndex> index = find_index<T>();

        if(index.has_value()) // component already registered
            return index;

        if (component_pools_.size() == kMaxComponents)
            return std::nullopt;

        component_pools_.push_back(PoolRecord{component_type_id<T>(), std::make_unique<ComponentPool<T>>()});
        
        index = static_cast<ComponentIndex>(component_pools_.size() - 1);
        return index;
    }

    template <typename T>
    inline std::optional<ComponentIndex> Registry::find_index() const noexcept
    {
        return find_index(component_type_id<T>());
    }

    template <typename T>
    inline const ComponentPool<T>* Registry::find_pool() const noexcept
    {
       std::optional<ComponentIndex> index = find_index<T>();

        if(!index.has_value())
            return nullptr;

        const PoolRecord& record = component_pools_[*index];
        assert(record.pool->type_id() == component_type_id<T>());
        return static_cast<const ComponentPool<T>*>(record.pool.get());
    }
    template <typename T>
    inline ComponentPool<T> *Registry::find_pool() noexcept
    {
        return const_cast<ComponentPool<T>*>(
            std::as_const(*this).find_pool<T>()
        );
    }

    template <typename T, typename... Args>
    inline T* Registry::add_component(Entity entity, Args &&...args)
    {
        if (!manager_.is_alive(entity))
            return nullptr;
        
        if (!register_component<T>().has_value())
            return nullptr;
            
        ComponentPool<T>* pool = find_pool<T>();
        assert(pool != nullptr);
        T* component = pool->try_emplace(entity, std::forward<Args>(args)...);
        return component;
    }

    template <typename T>
    inline T* Registry::get_component(Entity entity) noexcept
    {
        return const_cast<T*>(
            std::as_const(*this).get_component<T>(entity)
        );
    }

    template <typename T>
    inline const T* Registry::get_component(Entity entity) const noexcept
    {   
        if (!manager_.is_alive(entity))
            return nullptr;

        const ComponentPool<T>* pool = find_pool<T>();
        if (pool == nullptr)
            return nullptr;

        const T* component = pool->try_get(entity);
        
        return component;
    }

    template <typename T>
    inline bool Registry::remove_component(Entity entity) noexcept
    {
        if (!manager_.is_alive(entity))
            return false;
        
        ComponentPool<T>* pool = find_pool<T>();

        if (pool == nullptr)
            return false;

        return pool->erase(entity);
    }

    template <typename T>
    inline bool Registry::has_component(Entity entity) const noexcept
    {
        return get_component<T>(entity) != nullptr;
    }
    
    template <typename T, typename Fn>
    inline void Registry::for_each(Fn &&fn)
    {
        ComponentPool<T>* pool = find_pool<T>();
        if (pool == nullptr)
            return;
        
        for(int i = 0; i < pool->size(); ++i) {
            fn(pool->entities(), pool->components());
        }
    }
}
