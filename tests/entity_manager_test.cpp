#include <gtest/gtest.h>

#include "ecs/constants.hpp"
#include "ecs/entity_manager.hpp"
#include "ecs/types.hpp"

#include <vector>

namespace {

// EntityManager is neither copyable nor movable, so it lives as a fixture
// member. A fresh instance per test keeps the index/generation bookkeeping of
// one case from leaking into the next.
class EntityManagerTest : public ::testing::Test {
protected:
    ecs::EntityManager manager_;
};

}  // namespace


TEST_F(EntityManagerTest, NewManagerHasNoEntities) {
    EXPECT_EQ(manager_.alive_count(), 0u);
    EXPECT_FALSE(manager_.is_alive(ecs::Entity{}));
}

TEST_F(EntityManagerTest, CreateEntityReturnsAliveHandle) {
    const ecs::Entity entity = manager_.create_entity();

    EXPECT_TRUE(entity.is_valid());
    EXPECT_TRUE(manager_.is_alive(entity));
    EXPECT_EQ(entity.index, 0u);
    EXPECT_EQ(entity.generation, ecs::kFirstValidGeneration);
    EXPECT_EQ(manager_.alive_count(), 1u);
}

TEST_F(EntityManagerTest, CreateEntityHandsOutDistinctIndices) {
    const ecs::Entity a = manager_.create_entity();
    const ecs::Entity b = manager_.create_entity();
    const ecs::Entity c = manager_.create_entity();

    EXPECT_EQ(a.index, 0u);
    EXPECT_EQ(b.index, 1u);
    EXPECT_EQ(c.index, 2u);

    // Fresh slots all start at the first valid generation.
    EXPECT_EQ(a.generation, ecs::kFirstValidGeneration);
    EXPECT_EQ(b.generation, ecs::kFirstValidGeneration);
    EXPECT_EQ(c.generation, ecs::kFirstValidGeneration);

    EXPECT_EQ(manager_.alive_count(), 3u);
}

TEST_F(EntityManagerTest, DestroyEntityMarksItNotAlive) {
    const ecs::Entity entity = manager_.create_entity();

    EXPECT_TRUE(manager_.destroy_entity(entity));
    EXPECT_FALSE(manager_.is_alive(entity));
    EXPECT_EQ(manager_.alive_count(), 0u);
}

// Indices are a scarce resource (capped at kMaxEntities), so a destroyed index
// goes back on the free list. The generation bump is what keeps old handles to
// that index distinguishable from the new occupant.
TEST_F(EntityManagerTest, DestroyRecyclesTheIndexWithAnIncrementedGeneration) {
    const ecs::Entity first = manager_.create_entity();
    ASSERT_TRUE(manager_.destroy_entity(first));

    const ecs::Entity second = manager_.create_entity();

    EXPECT_EQ(second.index, first.index);
    EXPECT_EQ(second.generation, first.generation + 1);
    EXPECT_TRUE(manager_.is_alive(second));
    EXPECT_EQ(manager_.alive_count(), 1u);
}

// The whole point of carrying a generation in the handle: after an index is
// recycled, the old handle must not resolve to the new entity.
TEST_F(EntityManagerTest, StaleHandleIsNotAliveAfterRecycle) {
    const ecs::Entity stale = manager_.create_entity();
    ASSERT_TRUE(manager_.destroy_entity(stale));
    const ecs::Entity fresh = manager_.create_entity();

    ASSERT_EQ(stale.index, fresh.index);  // same slot...
    EXPECT_FALSE(manager_.is_alive(stale));  // ...but not the same entity
    EXPECT_TRUE(manager_.is_alive(fresh));
}

TEST_F(EntityManagerTest, FreeIndicesAreReusedBeforeNewOnesAreAllocated) {
    const ecs::Entity a = manager_.create_entity();
    const ecs::Entity b = manager_.create_entity();
    ASSERT_TRUE(manager_.destroy_entity(a));
    ASSERT_TRUE(manager_.destroy_entity(b));

    // Free list is a stack, so the most recently freed index comes back first.
    EXPECT_EQ(manager_.create_entity().index, b.index);
    EXPECT_EQ(manager_.create_entity().index, a.index);
    EXPECT_EQ(manager_.alive_count(), 2u);
}

// alive_count_ is maintained by hand, so a double-destroy that slipped past the
// liveness check would decrement it permanently and corrupt every later count.
TEST_F(EntityManagerTest, DestroyReportsFailureWithoutChangingTheCount) {
    const ecs::Entity entity = manager_.create_entity();
    ASSERT_TRUE(manager_.destroy_entity(entity));
    ASSERT_EQ(manager_.alive_count(), 0u);

    EXPECT_FALSE(manager_.destroy_entity(entity));                           // already dead
    EXPECT_FALSE(manager_.destroy_entity(ecs::Entity{}));                    // invalid handle
    EXPECT_FALSE(manager_.destroy_entity(ecs::Entity{0, 99}));               // wrong generation
    EXPECT_FALSE(manager_.destroy_entity(ecs::Entity{12345, 1}));            // never created

    EXPECT_EQ(manager_.alive_count(), 0u);
}

TEST_F(EntityManagerTest, IsAliveRejectsIndicesBeyondTheRecordTable) {
    ASSERT_TRUE(manager_.create_entity().is_valid());

    EXPECT_FALSE(manager_.is_alive(ecs::Entity{1, 1}));                 // record never allocated
    EXPECT_FALSE(manager_.is_alive(ecs::Entity{ecs::kMaxEntities, 1}));
    EXPECT_FALSE(manager_.is_alive(ecs::Entity{ecs::kInvalidIndex, 1}));
}

// The capacity cliff. create_entity() reports exhaustion by returning an
// invalid handle rather than throwing, so a caller that forgets to check
// is_valid() would otherwise feed Entity{kInvalidIndex, 0} straight into a pool.
TEST_F(EntityManagerTest, ReachingMaxEntitiesReturnsAnInvalidHandle) {
    std::vector<ecs::Entity> entities;
    entities.reserve(ecs::kMaxEntities);
    for (std::uint32_t i = 0; i < ecs::kMaxEntities; ++i) {
        const ecs::Entity entity = manager_.create_entity();
        ASSERT_TRUE(entity.is_valid()) << "ran out of entities early at " << i;
        entities.push_back(entity);
    }
    ASSERT_EQ(manager_.alive_count(), ecs::kMaxEntities);

    const ecs::Entity overflow = manager_.create_entity();
    EXPECT_FALSE(overflow.is_valid());
    EXPECT_FALSE(manager_.is_alive(overflow));
    EXPECT_EQ(manager_.alive_count(), ecs::kMaxEntities);  // no phantom increment

    // Freeing one slot must make the manager usable again.
    ASSERT_TRUE(manager_.destroy_entity(entities.front()));
    const ecs::Entity recovered = manager_.create_entity();
    EXPECT_TRUE(recovered.is_valid());
    EXPECT_EQ(recovered.index, entities.front().index);
}
