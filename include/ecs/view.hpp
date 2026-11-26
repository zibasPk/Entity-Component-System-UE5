#pragma once
#include <ecs/component_pool.hpp>
#include <ecs/types.hpp>
#include <span>
#include <tuple>
#include <utility>
#include <cstddef>
#include <algorithm>

namespace ecs {
    /// @brief A view captures the pools that exist when it is created. If a component type has never been registered at that point, the view stays empty even after components of that type are added. Create views right before using them.
    /// @tparam ...Ts 
    template <typename... Ts> class View {
        static_assert(sizeof...(Ts) > 0,
            "A view must name at least one component type.");
        public:
            explicit View(ComponentPool<Ts>*... pools) noexcept: pools_{pools...}{}

            /// @brief Applies a callback to each entity in the view that has all the components specified by the view. 
            /// - Allowed: creating entities, and adding or removing components of types not in this view.
            /// - For types in this view: only removing them from the current entity, or destroying the current entity.
            /// - Not allowed: anything else that touches this view's types, such as removing them from other entities or adding them to any entity. Adding can reallocate components_ and leave the references passed to fn pointing at freed memory.
            ///- Destroying any entity other than the current one counts as "touching this view's types", so it's not allowed. Name it explicitly, because it's the easiest one to do by accident.
            template <typename Fn> void each(Fn&& fn) const; 
            std::size_t size_hint() const noexcept;
            /// @brief Checks wether there is any empty component pool in the view
            /// @return true if at least componentpool is empty, false otherwise
            bool empty() const noexcept;
        private:
            /// @return the pool with the smallest entity list 
            const IComponentPool* lead_pool() const noexcept;
            std::tuple<ComponentPool<Ts>*...> pools_;
    };

    template <typename... Ts>
    inline std::size_t View<Ts...>::size_hint() const noexcept
    {
        auto pool = lead_pool();
        if(pool == nullptr)
            return 0;
        return pool->entities().size();
    }

    template <typename... Ts>
    inline bool View<Ts...>::empty() const noexcept
    {
        auto pool = lead_pool();
        if(pool == nullptr)
            return true; 
        return pool->entities().empty();
    }

    template <typename... Ts>
    inline const IComponentPool *View<Ts...>::lead_pool() const noexcept
    {
        return std::apply([](ComponentPool<Ts>*... pools) -> const IComponentPool* {

            if ((... || (pools == nullptr)))
                return nullptr;

            return std::min(
                {static_cast<const IComponentPool*>(pools)...},
                [](const IComponentPool* lhs, const IComponentPool* rhs) noexcept {
                    return lhs->entities().size() < rhs->entities().size();
                });
        }, pools_);
    }

    template <typename... Ts>
    template <typename Fn>
    void View<Ts...>::each(Fn&& fn) const
    {
        std::apply([&](ComponentPool<Ts>*... pools) {
            const IComponentPool* smallest_pool = lead_pool();
            if (smallest_pool == nullptr)
                return;

            auto entities = smallest_pool->entities();
            for (std::size_t i = entities.size(); i > 0 ; --i)
            {
                entities = smallest_pool->entities();
                if (i > entities.size())
                    continue;
                const Entity entity = entities[i-1];
                if (!(... && pools->contains(entity)))
                    continue;
                fn(entity, *pools->try_get(entity)...);
            }

        }, pools_);
    }
}