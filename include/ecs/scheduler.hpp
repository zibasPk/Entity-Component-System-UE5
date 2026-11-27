#pragma once
#include <vector>
#include <memory>
#include <ecs/registry.hpp>
#include <ecs/system_base.hpp>

namespace ecs {
    class Scheduler {
    public:
        template <typename T, typename... Args>
        T& add_system(Args&&... args);

        void update(Registry& registry, float delta_seconds);

    private:
        std::vector<std::unique_ptr<ISystem>> systems_;
    };
}