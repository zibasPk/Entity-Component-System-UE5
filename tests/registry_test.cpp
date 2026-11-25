#include <gtest/gtest.h>

#include "ecs/component_type_id.hpp"
#include "ecs/constants.hpp"
#include "ecs/registry.hpp"
#include "ecs/types.hpp"

#include <utility>
#include <vector>

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct Health {
    int points = 0;
};

// One definition yields as many distinct component types as we need: each
// Dummy<N> is its own type, so each gets its own id and its own pool. Used to
// drive the registry past kMaxComponents without writing 65 structs by hand.
template <int N>
struct Dummy {
    int value = 0;
};

// Fold expression over the comma operator: the pack expansion runs the
// registration once per N in the sequence, tallying successes and refusals.
template <int... Is>
void register_dummies(ecs::Registry& registry, int& registered, int& refused,
                      std::integer_sequence<int, Is...>) {
    ((registry.register_component<Dummy<Is>>().has_value() ? ++registered : ++refused), ...);
}

// Registry is neither copyable nor movable (EntityManager deletes both), so it
// lives as a fixture member. A fresh registry per test matters more here than
// anywhere else: pool indices are assigned in registration order, so a shared
// instance would make these tests depend on execution order.
class RegistryTest : public ::testing::Test {
protected:
    ecs::Registry registry_;
};

}  // namespace


// ---------------------------------------------------------------- registration

TEST_F(RegistryTest, RegisterComponentAssignsDenseIndicesInOrder) {
    const std::optional<ecs::ComponentIndex> first  = registry_.register_component<Position>();
    const std::optional<ecs::ComponentIndex> second = registry_.register_component<Velocity>();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, 0u);
    EXPECT_EQ(*second, 1u);
}

// Idempotent on purpose: a system can register the components it needs during
// its own init without caring whether another system got there first.
TEST_F(RegistryTest, RegisterComponentIsIdempotent) {
    const std::optional<ecs::ComponentIndex> first = registry_.register_component<Position>();
    ASSERT_TRUE(first.has_value());

    const std::optional<ecs::ComponentIndex> again = registry_.register_component<Position>();

    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(*again, *first);

    // ...and it must not have created a second pool for the same type.
    const std::optional<ecs::ComponentIndex> next = registry_.register_component<Velocity>();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(*next, 1u);
}

TEST_F(RegistryTest, LookupsFindNothingBeforeRegistration) {
    EXPECT_FALSE(registry_.find_index<Position>().has_value());
    EXPECT_FALSE(registry_.find_index(ecs::component_type_id<Position>()).has_value());
    EXPECT_EQ(registry_.find_pool<Position>(), nullptr);

    const ecs::Registry& const_registry = registry_;
    EXPECT_EQ(const_registry.find_pool<Position>(), nullptr);
}

// The type-id overload is the entry point a deserializer would use: it has an
// id read from a file and no static type to hand to the template version.
TEST_F(RegistryTest, FindIndexByTypeIdAgreesWithTheTemplateOverload) {
    ASSERT_TRUE(registry_.register_component<Position>().has_value());
    ASSERT_TRUE(registry_.register_component<Velocity>().has_value());

    EXPECT_EQ(registry_.find_index(ecs::component_type_id<Position>()),
              registry_.find_index<Position>());
    EXPECT_EQ(registry_.find_index(ecs::component_type_id<Velocity>()),
              registry_.find_index<Velocity>());
    EXPECT_FALSE(registry_.find_index(ecs::component_type_id<Health>()).has_value());
}

TEST_F(RegistryTest, FindPoolReturnsTheCorrectlyTypedPool) {
    ASSERT_TRUE(registry_.register_component<Position>().has_value());
    ASSERT_TRUE(registry_.register_component<Velocity>().has_value());

    ecs::ComponentPool<Position>* positions = registry_.find_pool<Position>();
    ecs::ComponentPool<Velocity>* velocities = registry_.find_pool<Velocity>();

    ASSERT_NE(positions, nullptr);
    ASSERT_NE(velocities, nullptr);
    EXPECT_EQ(positions->type_id(), ecs::component_type_id<Position>());
    EXPECT_EQ(velocities->type_id(), ecs::component_type_id<Velocity>());
}

// kMaxComponents is 64 because the planned entity signature is a 64-bit mask:
// handing out index 64 would mean shifting past the width of that type, which
// is undefined behaviour rather than a tidy failure. The cap must hold.
TEST_F(RegistryTest, RegisteringPastMaxComponentsIsRefused) {
    int registered = 0;
    int refused = 0;
    register_dummies(registry_, registered, refused,
                     std::make_integer_sequence<int, static_cast<int>(ecs::kMaxComponents) + 4>{});

    EXPECT_EQ(registered, static_cast<int>(ecs::kMaxComponents));
    EXPECT_EQ(refused, 4);

    // A type that never got a slot must report absent rather than mis-resolve.
    EXPECT_FALSE(registry_.find_index<Position>().has_value());
    EXPECT_EQ(registry_.find_pool<Position>(), nullptr);
}


// ------------------------------------------------------------- add_component

TEST_F(RegistryTest, AddComponentAutoRegistersThePool) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_FALSE(registry_.find_index<Position>().has_value());

    Position* added = registry_.add_component<Position>(entity, Position{1.5f, 2.5f});

    ASSERT_NE(added, nullptr);
    EXPECT_FLOAT_EQ(added->x, 1.5f);
    EXPECT_FLOAT_EQ(added->y, 2.5f);

    // The pool now exists even though nobody registered it explicitly.
    ASSERT_TRUE(registry_.find_index<Position>().has_value());
    EXPECT_EQ(*registry_.find_index<Position>(), 0u);
}

TEST_F(RegistryTest, AddComponentRejectsADuplicate) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);

    EXPECT_EQ(registry_.add_component<Position>(entity, Position{9.0f, 9.0f}), nullptr);

    EXPECT_FLOAT_EQ(registry_.get_component<Position>(entity)->x, 1.0f);
    EXPECT_EQ(registry_.find_pool<Position>()->size(), 1u);
}

// Only the registry can answer "is this entity alive" -- a pool holds no
// reference to the EntityManager. Without this guard, a stale handle would
// attach a component that destroy_entity has already swept, permanently
// occupying that index in the pool's sparse array and blocking the entity that
// later recycles the index from ever acquiring the same component type.
TEST_F(RegistryTest, AddComponentRejectsADeadEntity) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_TRUE(registry_.destroy_entity(entity));

    EXPECT_EQ(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);
    EXPECT_FALSE(registry_.has_component<Position>(entity));
}

TEST_F(RegistryTest, AddComponentRejectsInvalidAndStaleHandles) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);

    EXPECT_EQ(registry_.add_component<Velocity>(ecs::Entity{}, Velocity{1.0f, 1.0f}), nullptr);
    EXPECT_EQ(registry_.add_component<Velocity>(
                  ecs::Entity{entity.index, static_cast<ecs::Generation>(entity.generation + 1)},
                  Velocity{1.0f, 1.0f}),
              nullptr);
}

TEST_F(RegistryTest, AddComponentSupportsSeveralTypesOnOneEntity) {
    const ecs::Entity entity = registry_.create_entity();

    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 2.0f}), nullptr);
    ASSERT_NE(registry_.add_component<Velocity>(entity, Velocity{3.0f, 4.0f}), nullptr);
    ASSERT_NE(registry_.add_component<Health>(entity, Health{100}), nullptr);

    EXPECT_FLOAT_EQ(registry_.get_component<Position>(entity)->x, 1.0f);
    EXPECT_FLOAT_EQ(registry_.get_component<Velocity>(entity)->dx, 3.0f);
    EXPECT_EQ(registry_.get_component<Health>(entity)->points, 100);
}


// ------------------------------------------------------------- get_component

TEST_F(RegistryTest, GetComponentRoundTripsTheStoredValue) {
    const ecs::Entity entity = registry_.create_entity();
    Position* added = registry_.add_component<Position>(entity, Position{1.5f, 2.5f});
    ASSERT_NE(added, nullptr);

    Position* fetched = registry_.get_component<Position>(entity);

    ASSERT_NE(fetched, nullptr);
    EXPECT_EQ(fetched, added);  // same storage, not a copy
    EXPECT_FLOAT_EQ(fetched->x, 1.5f);
}

TEST_F(RegistryTest, GetComponentReturnsAMutableView) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);

    registry_.get_component<Position>(entity)->x = 42.0f;

    EXPECT_FLOAT_EQ(registry_.get_component<Position>(entity)->x, 42.0f);
}

// The non-const overload delegates to the const one through a const_cast, so
// both must agree about which component they resolve to.
TEST_F(RegistryTest, ConstAndNonConstGetComponentAgree) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{7.0f, 8.0f}), nullptr);

    const ecs::Registry& const_registry = registry_;
    const Position* read_only = const_registry.get_component<Position>(entity);

    ASSERT_NE(read_only, nullptr);
    EXPECT_EQ(read_only, registry_.get_component<Position>(entity));
    EXPECT_FLOAT_EQ(read_only->y, 8.0f);
}

TEST_F(RegistryTest, GetComponentReturnsNullForMissingPoolOrComponent) {
    const ecs::Entity entity = registry_.create_entity();

    EXPECT_EQ(registry_.get_component<Position>(entity), nullptr);  // pool not registered

    ASSERT_TRUE(registry_.register_component<Position>().has_value());
    EXPECT_EQ(registry_.get_component<Position>(entity), nullptr);  // pool empty
}

TEST_F(RegistryTest, GetComponentRejectsDeadAndStaleHandles) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);

    const ecs::Entity stale{entity.index, static_cast<ecs::Generation>(entity.generation + 1)};
    EXPECT_EQ(registry_.get_component<Position>(stale), nullptr);

    ASSERT_TRUE(registry_.destroy_entity(entity));
    EXPECT_EQ(registry_.get_component<Position>(entity), nullptr);
}


// ---------------------------------------------- remove_component / has_component

TEST_F(RegistryTest, RemoveComponentDetachesItOnce) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);

    EXPECT_TRUE(registry_.remove_component<Position>(entity));

    EXPECT_FALSE(registry_.has_component<Position>(entity));
    EXPECT_EQ(registry_.get_component<Position>(entity), nullptr);
    EXPECT_EQ(registry_.find_pool<Position>()->size(), 0u);

    EXPECT_FALSE(registry_.remove_component<Position>(entity));  // idempotent-by-failure
}

TEST_F(RegistryTest, RemoveComponentLeavesOtherComponentsAlone) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 2.0f}), nullptr);
    ASSERT_NE(registry_.add_component<Velocity>(entity, Velocity{3.0f, 4.0f}), nullptr);

    ASSERT_TRUE(registry_.remove_component<Position>(entity));

    EXPECT_FALSE(registry_.has_component<Position>(entity));
    EXPECT_TRUE(registry_.has_component<Velocity>(entity));
    EXPECT_FLOAT_EQ(registry_.get_component<Velocity>(entity)->dx, 3.0f);
}

TEST_F(RegistryTest, RemoveComponentRejectsDeadEntitiesAndMissingPools) {
    const ecs::Entity entity = registry_.create_entity();

    EXPECT_FALSE(registry_.remove_component<Position>(entity));  // pool not registered

    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);
    ASSERT_TRUE(registry_.destroy_entity(entity));
    EXPECT_FALSE(registry_.remove_component<Position>(entity));  // dead entity
}

TEST_F(RegistryTest, HasComponentTracksAttachment) {
    const ecs::Entity entity = registry_.create_entity();

    EXPECT_FALSE(registry_.has_component<Position>(entity));

    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 1.0f}), nullptr);
    EXPECT_TRUE(registry_.has_component<Position>(entity));
    EXPECT_FALSE(registry_.has_component<Velocity>(entity));

    ASSERT_TRUE(registry_.remove_component<Position>(entity));
    EXPECT_FALSE(registry_.has_component<Position>(entity));
}


// ----------------------------------------------------------- entity lifecycle

TEST_F(RegistryTest, IsAliveTracksTheEntityLifecycle) {
    const ecs::Entity entity = registry_.create_entity();

    EXPECT_TRUE(registry_.is_alive(entity));
    EXPECT_TRUE(registry_.destroy_entity(entity));
    EXPECT_FALSE(registry_.is_alive(entity));
    EXPECT_FALSE(registry_.destroy_entity(entity));  // already gone
    EXPECT_FALSE(registry_.is_alive(ecs::Entity{}));
}

// destroy_entity sweeps every pool through the IComponentPool interface, so
// this is the only coverage of that loop. Checked against pool sizes rather
// than has_component(), which would short-circuit on the liveness check and
// pass even if the components were still sitting in their pools.
TEST_F(RegistryTest, DestroyEntityClearsComponentsFromEveryPool) {
    const ecs::Entity entity = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(entity, Position{1.0f, 2.0f}), nullptr);
    ASSERT_NE(registry_.add_component<Velocity>(entity, Velocity{3.0f, 4.0f}), nullptr);
    ASSERT_NE(registry_.add_component<Health>(entity, Health{100}), nullptr);
    ASSERT_EQ(registry_.find_pool<Position>()->size(), 1u);
    ASSERT_EQ(registry_.find_pool<Velocity>()->size(), 1u);
    ASSERT_EQ(registry_.find_pool<Health>()->size(), 1u);

    ASSERT_TRUE(registry_.destroy_entity(entity));

    EXPECT_EQ(registry_.find_pool<Position>()->size(), 0u);
    EXPECT_EQ(registry_.find_pool<Velocity>()->size(), 0u);
    EXPECT_EQ(registry_.find_pool<Health>()->size(), 0u);
}

TEST_F(RegistryTest, DestroyEntityLeavesOtherEntitiesComponentsIntact) {
    const ecs::Entity keep = registry_.create_entity();
    const ecs::Entity drop = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(keep, Position{1.0f, 1.0f}), nullptr);
    ASSERT_NE(registry_.add_component<Position>(drop, Position{2.0f, 2.0f}), nullptr);

    ASSERT_TRUE(registry_.destroy_entity(drop));

    ASSERT_NE(registry_.get_component<Position>(keep), nullptr);
    EXPECT_FLOAT_EQ(registry_.get_component<Position>(keep)->x, 1.0f);
    EXPECT_EQ(registry_.find_pool<Position>()->size(), 1u);
}

// The scenario that ties the generation scheme to the pools. If a destroyed
// entity's component were left behind, the pool's sparse array would still
// consider the index occupied, and SparseSet::insert -- which compares indices
// without regard to generation -- would refuse the recycled entity's component
// forever.
TEST_F(RegistryTest, RecycledIndexCanTakeAFreshComponent) {
    const ecs::Entity first = registry_.create_entity();
    ASSERT_NE(registry_.add_component<Position>(first, Position{1.0f, 1.0f}), nullptr);
    ASSERT_TRUE(registry_.destroy_entity(first));

    const ecs::Entity recycled = registry_.create_entity();
    ASSERT_EQ(recycled.index, first.index);
    ASSERT_NE(recycled.generation, first.generation);

    Position* added = registry_.add_component<Position>(recycled, Position{5.0f, 6.0f});

    ASSERT_NE(added, nullptr);
    EXPECT_FLOAT_EQ(added->x, 5.0f);
    EXPECT_FLOAT_EQ(registry_.get_component<Position>(recycled)->y, 6.0f);
    EXPECT_EQ(registry_.get_component<Position>(first), nullptr);  // old handle stays dead
}

TEST_F(RegistryTest, ManyEntitiesKeepTheirOwnComponents) {
    constexpr int kCount = 64;
    std::vector<ecs::Entity> entities;
    entities.reserve(kCount);

    for (int i = 0; i < kCount; ++i) {
        const ecs::Entity entity = registry_.create_entity();
        ASSERT_TRUE(entity.is_valid());
        ASSERT_NE(registry_.add_component<Health>(entity, Health{i}), nullptr);
        entities.push_back(entity);
    }

    // Remove every other one, forcing a long run of swap-and-pop moves.
    for (int i = 0; i < kCount; i += 2) {
        ASSERT_TRUE(registry_.remove_component<Health>(entities[i]));
    }

    for (int i = 0; i < kCount; ++i) {
        const Health* health = registry_.get_component<Health>(entities[i]);
        if (i % 2 == 0) {
            EXPECT_EQ(health, nullptr) << "entity " << i << " should have lost its component";
        } else {
            ASSERT_NE(health, nullptr) << "entity " << i << " lost its component";
            EXPECT_EQ(health->points, i) << "entity " << i << " got another entity's component";
        }
    }
    EXPECT_EQ(registry_.find_pool<Health>()->size(), static_cast<std::size_t>(kCount / 2));
}
