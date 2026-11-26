#pragma once
#include <ecs/component_pool.hpp>
#include <ecs/types.hpp>
#include <span>
#include <tuple>
#include <utility>
#include <cstddef>
#include <algorithm>

namespace ecs {
    template <typename... Ts> class View {
        static_assert(sizeof...(Ts) > 0,
            "A view must name at least one component type.");
        public:
            explicit View(ComponentPool<Ts>*... pools) noexcept: pools_{pools...}{}

            /// @brief Applies a function to each entity in the view that has all the components specified by the view.
            template <typename Fn> void each(Fn&& fn) const; 
            std::size_t size_hint() const noexcept;
            /// @brief Checks wether there is any empty component pool in the view
            /// @return true if at least componentpool is empty, false otherwise
            bool empty() const noexcept;
        private:
            /// @return entity list derived from the smallest pool 
            std::span<const Entity> lead_entities() const noexcept;
            std::tuple<ComponentPool<Ts>*...> pools_;
    };

    template <typename... Ts>
    inline std::size_t View<Ts...>::size_hint() const noexcept
    {
        return lead_entities().size();
    }

    template <typename... Ts>
    inline bool View<Ts...>::empty() const noexcept
    {
        return lead_entities().empty();
    }

    template <typename... Ts>
    inline std::span<const Entity> View<Ts...>::lead_entities() const noexcept
    {
        return std::apply([](ComponentPool<Ts>*... pools) -> std::span<const Entity> {

            if ((... || (pools == nullptr)))
                return {};

            return std::min(
                {std::span<const Entity>{pools->entities()}...},
                [](std::span<const Entity> lhs, std::span<const Entity> rhs) noexcept {
                    return lhs.size() < rhs.size();
                });
        }, pools_);
    }

    template <typename... Ts>
    template <typename Fn>
    void View<Ts...>::each(Fn&& fn) const
    {
        std::apply([&](ComponentPool<Ts>*... pools) {
            
            if ((... || (pools == nullptr)))
                return;

            for (const Entity entity : lead_entities()) {
                if (!(... && pools->contains(entity)))
                    continue;

                fn(entity, *pools->try_get(entity)...);
            }
        }, pools_);
    }
    }