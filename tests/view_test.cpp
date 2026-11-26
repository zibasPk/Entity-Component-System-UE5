#include <ecs/registry.hpp>
#include <ecs/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace {

struct Position { float x = 0.0f; float y = 0.0f; };
struct Velocity { float dx = 0.0f; float dy = 0.0f; };
struct Health   { float max = 0.0f; float current = 0.0f; };

// Registry is neither copyable nor movable, so it lives as a fixture member;
// every TEST_F gets a fresh one.
class ViewTest : public ::testing::Test {
protected:
    // Entities must come from the registry: add_component rejects any entity
    // the EntityManager doesn't consider alive.
    ecs::Entity make_entity() { return registry_.create_entity(); }

    // Collects every entity a View<Ts...> visits, in visit order.
    template <typename... Ts>
    std::vector<ecs::Entity> visited_by(ecs::View<Ts...> view) {
        std::vector<ecs::Entity> visited;
        view.each([&visited](ecs::Entity entity, Ts&...) { visited.push_back(entity); });
        return visited;
    }

    ecs::Registry registry_;
};

}  // namespace


// ------------------------------------------------------------- empty / missing

TEST_F(ViewTest, EmptyWhenComponentNeverRegistered) {
    auto view = registry_.view<Position>();

    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.size_hint(), 0u);

    bool called = false;
    view.each([&called](ecs::Entity, Position&) { called = true; });

    EXPECT_FALSE(called);
}

TEST_F(ViewTest, EmptyWhenComponentRegisteredButUnused) {
    ASSERT_TRUE(registry_.register_component<Position>().has_value());

    auto view = registry_.view<Position>();

    EXPECT_TRUE(view.empty());
    EXPECT_THAT(visited_by(view), IsEmpty());
}

TEST_F(ViewTest, EmptyWhenOneOfTheComponentsWasNeverRegistered) {
    const ecs::Entity entity = make_entity();
    registry_.add_component<Position>(entity, 1.0f, 2.0f);
    // Velocity is never added, so its pool is nullptr inside the view.

    auto view = registry_.view<Position, Velocity>();

    EXPECT_TRUE(view.empty());
    EXPECT_THAT(visited_by(view), IsEmpty());
}


// ------------------------------------------------------------ single component

TEST_F(ViewTest, SingleComponentVisitsEveryOwner) {
    const ecs::Entity e1 = make_entity();
    const ecs::Entity e2 = make_entity();
    const ecs::Entity e3 = make_entity();

    registry_.add_component<Position>(e1, 10.0f, 20.0f);
    registry_.add_component<Position>(e2, 10.0f, 20.0f);
    registry_.add_component<Position>(e3, 10.0f, 20.0f);

    auto view = registry_.view<Position>();

    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size_hint(), 3u);
    EXPECT_THAT(visited_by(view), UnorderedElementsAre(e1, e2, e3));
}

TEST_F(ViewTest, SingleComponentSkipsEntitiesWithoutIt) {
    const ecs::Entity with    = make_entity();
    const ecs::Entity without = make_entity();

    registry_.add_component<Position>(with, 1.0f, 1.0f);
    registry_.add_component<Velocity>(without, 1.0f, 1.0f);

    EXPECT_THAT(visited_by(registry_.view<Position>()), UnorderedElementsAre(with));
}


// ------------------------------------------------------------- multi component

TEST_F(ViewTest, MultiComponentVisitsOnlyTheIntersection) {
    const ecs::Entity both_a        = make_entity();
    const ecs::Entity both_b        = make_entity();
    const ecs::Entity only_position = make_entity();
    const ecs::Entity only_velocity = make_entity();

    registry_.add_component<Position>(both_a, 1.0f, 1.0f);
    registry_.add_component<Velocity>(both_a, 1.0f, 1.0f);
    registry_.add_component<Position>(both_b, 1.0f, 1.0f);
    registry_.add_component<Velocity>(both_b, 1.0f, 1.0f);
    registry_.add_component<Position>(only_position, 1.0f, 1.0f);
    registry_.add_component<Velocity>(only_velocity, 1.0f, 1.0f);

    EXPECT_THAT(visited_by(registry_.view<Position, Velocity>()),
                UnorderedElementsAre(both_a, both_b));
}

TEST_F(ViewTest, TemplateArgumentOrderDoesNotChangeTheResult) {
    const ecs::Entity both          = make_entity();
    const ecs::Entity only_position = make_entity();

    registry_.add_component<Position>(both, 1.0f, 1.0f);
    registry_.add_component<Velocity>(both, 1.0f, 1.0f);
    registry_.add_component<Position>(only_position, 1.0f, 1.0f);

    EXPECT_THAT(visited_by(registry_.view<Position, Velocity>()), UnorderedElementsAre(both));
    EXPECT_THAT(visited_by(registry_.view<Velocity, Position>()), UnorderedElementsAre(both));
}

TEST_F(ViewTest, ThreeComponentIntersection) {
    const ecs::Entity all_three = make_entity();
    const ecs::Entity two_only  = make_entity();

    registry_.add_component<Position>(all_three, 1.0f, 1.0f);
    registry_.add_component<Velocity>(all_three, 1.0f, 1.0f);
    registry_.add_component<Health>(all_three, 100.0f, 100.0f);
    registry_.add_component<Position>(two_only, 1.0f, 1.0f);
    registry_.add_component<Velocity>(two_only, 1.0f, 1.0f);

    EXPECT_THAT(visited_by(registry_.view<Position, Velocity, Health>()),
                UnorderedElementsAre(all_three));
}


// ------------------------------------------------------------------ size_hint

// size_hint is the size of the smallest pool: an upper bound on how many
// entities each() visits, not an exact count.
TEST_F(ViewTest, SizeHintIsTheSmallestPoolSize) {
    const ecs::Entity e1 = make_entity();
    const ecs::Entity e2 = make_entity();
    const ecs::Entity e3 = make_entity();

    registry_.add_component<Position>(e1, 1.0f, 1.0f);
    registry_.add_component<Position>(e2, 1.0f, 1.0f);
    registry_.add_component<Position>(e3, 1.0f, 1.0f);
    registry_.add_component<Velocity>(e1, 1.0f, 1.0f);

    // Extra parentheses: the preprocessor doesn't understand <>, so the comma in
    // view<Position, Velocity> would otherwise split the macro's arguments.
    EXPECT_EQ((registry_.view<Position, Velocity>().size_hint()), 1u);
}

TEST_F(ViewTest, SizeHintCanExceedTheVisitedCount) {
    const ecs::Entity e1 = make_entity();
    const ecs::Entity e2 = make_entity();

    // Disjoint sets: both pools have one entry, the intersection is empty.
    registry_.add_component<Position>(e1, 1.0f, 1.0f);
    registry_.add_component<Velocity>(e2, 1.0f, 1.0f);

    auto view = registry_.view<Position, Velocity>();

    EXPECT_EQ(view.size_hint(), 1u);
    EXPECT_FALSE(view.empty());  // empty() only means "some pool is empty"
    EXPECT_THAT(visited_by(view), IsEmpty());
}


// -------------------------------------------------------- component arguments

TEST_F(ViewTest, CallbackReceivesTheVisitedEntitysComponents) {
    const ecs::Entity e1 = make_entity();
    const ecs::Entity e2 = make_entity();

    registry_.add_component<Position>(e1, 1.0f, 2.0f);
    registry_.add_component<Velocity>(e1, 3.0f, 4.0f);
    registry_.add_component<Position>(e2, 5.0f, 6.0f);
    registry_.add_component<Velocity>(e2, 7.0f, 8.0f);

    std::size_t visits = 0;
    registry_.view<Position, Velocity>().each(
        [&](ecs::Entity entity, Position& position, Velocity& velocity) {
            ++visits;
            // Each reference must be the component stored for *this* entity,
            // not whatever sits at the same dense index in another pool.
            EXPECT_EQ(&position, registry_.get_component<Position>(entity));
            EXPECT_EQ(&velocity, registry_.get_component<Velocity>(entity));
        });

    EXPECT_EQ(visits, 2u);
}

TEST_F(ViewTest, ComponentsMatchEvenWhenPoolsAreOrderedDifferently) {
    const ecs::Entity e1 = make_entity();
    const ecs::Entity e2 = make_entity();

    // Insert in opposite orders so e1/e2 sit at different dense indices in
    // each pool.
    registry_.add_component<Position>(e1, 1.0f, 1.0f);
    registry_.add_component<Position>(e2, 2.0f, 2.0f);
    registry_.add_component<Velocity>(e2, 20.0f, 20.0f);
    registry_.add_component<Velocity>(e1, 10.0f, 10.0f);

    registry_.view<Position, Velocity>().each(
        [&](ecs::Entity entity, Position& position, Velocity& velocity) {
            if (entity == e1) {
                EXPECT_FLOAT_EQ(position.x, 1.0f);
                EXPECT_FLOAT_EQ(velocity.dx, 10.0f);
            } else {
                EXPECT_FLOAT_EQ(position.x, 2.0f);
                EXPECT_FLOAT_EQ(velocity.dx, 20.0f);
            }
        });
}

TEST_F(ViewTest, WritesThroughReferencesArePersisted) {
    const ecs::Entity entity = make_entity();
    registry_.add_component<Position>(entity, 0.0f, 0.0f);
    registry_.add_component<Velocity>(entity, 1.5f, -2.0f);

    registry_.view<Position, Velocity>().each(
        [](ecs::Entity, Position& position, Velocity& velocity) {
            position.x += velocity.dx;
            position.y += velocity.dy;
        });

    const Position* position = registry_.get_component<Position>(entity);
    ASSERT_NE(position, nullptr);
    EXPECT_FLOAT_EQ(position->x, 1.5f);
    EXPECT_FLOAT_EQ(position->y, -2.0f);
}


// ---------------------------------------------------- structural changes after

TEST_F(ViewTest, SkipsEntityAfterComponentRemoved) {
    const ecs::Entity kept    = make_entity();
    const ecs::Entity removed = make_entity();

    registry_.add_component<Position>(kept, 1.0f, 1.0f);
    registry_.add_component<Velocity>(kept, 1.0f, 1.0f);
    registry_.add_component<Position>(removed, 1.0f, 1.0f);
    registry_.add_component<Velocity>(removed, 1.0f, 1.0f);

    ASSERT_TRUE(registry_.remove_component<Velocity>(removed));

    EXPECT_THAT(visited_by(registry_.view<Position, Velocity>()), UnorderedElementsAre(kept));
}

TEST_F(ViewTest, SkipsDestroyedEntity) {
    const ecs::Entity kept      = make_entity();
    const ecs::Entity destroyed = make_entity();

    registry_.add_component<Position>(kept, 1.0f, 1.0f);
    registry_.add_component<Position>(destroyed, 1.0f, 1.0f);

    ASSERT_TRUE(registry_.destroy_entity(destroyed));

    EXPECT_THAT(visited_by(registry_.view<Position>()), UnorderedElementsAre(kept));
}

TEST_F(ViewTest, RecycledIndexIsNotConfusedWithTheDestroyedEntity) {
    const ecs::Entity old_entity = make_entity();
    registry_.add_component<Position>(old_entity, 1.0f, 1.0f);
    ASSERT_TRUE(registry_.destroy_entity(old_entity));

    // The new entity may reuse old_entity's index with a newer generation.
    const ecs::Entity new_entity = make_entity();
    registry_.add_component<Velocity>(new_entity, 1.0f, 1.0f);

    EXPECT_THAT(visited_by(registry_.view<Position>()), IsEmpty());
    EXPECT_THAT(visited_by(registry_.view<Velocity>()), UnorderedElementsAre(new_entity));
}
