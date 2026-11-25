#include <gtest/gtest.h>

#include "ecs/component_pool.hpp"
#include "ecs/component_type_id.hpp"
#include "ecs/types.hpp"

#include <memory>
#include <string>
#include <type_traits>

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

// Handles are built by hand here: ComponentPool knows nothing about liveness,
// so it never needs an EntityManager to produce them.
constexpr ecs::Entity kA{0, 1};
constexpr ecs::Entity kB{1, 1};
constexpr ecs::Entity kC{2, 1};

class ComponentPoolTest : public ::testing::Test {
protected:
    ecs::ComponentPool<Position> pool_;

    // Fills the pool with three known components so the swap-and-pop tests can
    // check that the survivors still read back their own values.
    void seed_three() {
        ASSERT_NE(pool_.try_emplace(kA, Position{1.0f, 1.0f}), nullptr);
        ASSERT_NE(pool_.try_emplace(kB, Position{2.0f, 2.0f}), nullptr);
        ASSERT_NE(pool_.try_emplace(kC, Position{3.0f, 3.0f}), nullptr);
        ASSERT_EQ(pool_.size(), 3u);
    }
};

}  // namespace


TEST_F(ComponentPoolTest, NewPoolIsEmpty) {
    EXPECT_TRUE(pool_.is_empty());
    EXPECT_EQ(pool_.size(), 0u);
    EXPECT_FALSE(pool_.contains(kA));
    EXPECT_EQ(pool_.try_get(kA), nullptr);
}

TEST_F(ComponentPoolTest, TryEmplaceStoresAndReturnsTheComponent) {
    Position* stored = pool_.try_emplace(kA, Position{1.5f, 2.5f});

    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->x, 1.5f);
    EXPECT_FLOAT_EQ(stored->y, 2.5f);
    EXPECT_EQ(pool_.size(), 1u);
    EXPECT_FALSE(pool_.is_empty());
    EXPECT_TRUE(pool_.contains(kA));

    // The returned pointer aliases the stored component, not a copy of it.
    EXPECT_EQ(stored, pool_.try_get(kA));
}

TEST_F(ComponentPoolTest, TryEmplaceRejectsADuplicateWithoutMutating) {
    ASSERT_NE(pool_.try_emplace(kA, Position{1.0f, 1.0f}), nullptr);

    EXPECT_EQ(pool_.try_emplace(kA, Position{9.0f, 9.0f}), nullptr);

    EXPECT_EQ(pool_.size(), 1u);
    EXPECT_FLOAT_EQ(pool_.try_get(kA)->x, 1.0f);  // original value survives
}

TEST_F(ComponentPoolTest, TryGetRejectsAbsentAndStaleHandles) {
    ASSERT_NE(pool_.try_emplace(kA, Position{1.0f, 1.0f}), nullptr);

    EXPECT_EQ(pool_.try_get(kB), nullptr);                       // never inserted
    EXPECT_EQ(pool_.try_get(ecs::Entity{kA.index, 2}), nullptr);  // same slot, newer generation
    EXPECT_EQ(pool_.try_get(ecs::Entity{}), nullptr);            // invalid handle
}

TEST_F(ComponentPoolTest, ConstAndNonConstTryGetReturnTheSameComponent) {
    ASSERT_NE(pool_.try_emplace(kA, Position{4.0f, 5.0f}), nullptr);

    const ecs::ComponentPool<Position>& const_pool = pool_;
    const Position* read_only = const_pool.try_get(kA);
    Position* writable = pool_.try_get(kA);

    ASSERT_NE(read_only, nullptr);
    ASSERT_EQ(read_only, writable);

    // The non-const overload really is a mutable view of the same storage.
    writable->x = 42.0f;
    EXPECT_FLOAT_EQ(const_pool.try_get(kA)->x, 42.0f);
}

// This is what makes Registry::find_pool's assert meaningful: the pool reports
// the same id the registry stored for it, so a mismatched static_cast is caught
// rather than silently reinterpreting memory.
TEST_F(ComponentPoolTest, TypeIdMatchesTheFreeFunction) {
    EXPECT_EQ(pool_.type_id(), ecs::component_type_id<Position>());

    ecs::ComponentPool<float> other;
    EXPECT_NE(pool_.type_id(), other.type_id());
}

TEST_F(ComponentPoolTest, EraseLastElementNeedsNoRemap) {
    seed_three();

    EXPECT_TRUE(pool_.erase(kC));

    EXPECT_EQ(pool_.size(), 2u);
    EXPECT_FALSE(pool_.contains(kC));
    EXPECT_FLOAT_EQ(pool_.try_get(kA)->x, 1.0f);
    EXPECT_FLOAT_EQ(pool_.try_get(kB)->x, 2.0f);
}

// The invariant that costs the most if it breaks. Erasing a non-last element
// moves the last component into the hole, and SparseSet::erase independently
// performs the same move on its dense array. If the two ever disagree there is
// no crash -- a surviving entity just starts reading a different entity's
// component. These assertions are what would catch that.
TEST_F(ComponentPoolTest, EraseMiddleKeepsSurvivingComponentsIntact) {
    seed_three();

    EXPECT_TRUE(pool_.erase(kB));

    EXPECT_EQ(pool_.size(), 2u);
    EXPECT_FALSE(pool_.contains(kB));
    ASSERT_NE(pool_.try_get(kA), nullptr);
    ASSERT_NE(pool_.try_get(kC), nullptr);
    EXPECT_FLOAT_EQ(pool_.try_get(kA)->x, 1.0f);
    EXPECT_FLOAT_EQ(pool_.try_get(kC)->x, 3.0f);  // moved, but still its own value
}

TEST_F(ComponentPoolTest, EraseFirstKeepsSurvivingComponentsIntact) {
    seed_three();

    EXPECT_TRUE(pool_.erase(kA));

    EXPECT_EQ(pool_.size(), 2u);
    EXPECT_FALSE(pool_.contains(kA));
    EXPECT_FLOAT_EQ(pool_.try_get(kB)->x, 2.0f);
    EXPECT_FLOAT_EQ(pool_.try_get(kC)->x, 3.0f);
}

TEST_F(ComponentPoolTest, EraseEveryElementEmptiesThePool) {
    seed_three();

    EXPECT_TRUE(pool_.erase(kB));
    EXPECT_TRUE(pool_.erase(kA));
    EXPECT_TRUE(pool_.erase(kC));

    EXPECT_TRUE(pool_.is_empty());
    EXPECT_EQ(pool_.size(), 0u);
}

TEST_F(ComponentPoolTest, EraseReportsFailureWithoutMutating) {
    ASSERT_NE(pool_.try_emplace(kA, Position{1.0f, 1.0f}), nullptr);

    EXPECT_FALSE(pool_.erase(kB));                        // never inserted
    EXPECT_FALSE(pool_.erase(ecs::Entity{kA.index, 2}));  // stale generation
    EXPECT_FALSE(pool_.erase(ecs::Entity{}));             // invalid handle

    EXPECT_EQ(pool_.size(), 1u);
    EXPECT_TRUE(pool_.contains(kA));
}

TEST_F(ComponentPoolTest, EraseThenReinsertReusesTheSlot) {
    seed_three();
    ASSERT_TRUE(pool_.erase(kB));

    Position* reinserted = pool_.try_emplace(kB, Position{7.0f, 7.0f});

    ASSERT_NE(reinserted, nullptr);
    EXPECT_EQ(pool_.size(), 3u);
    EXPECT_FLOAT_EQ(pool_.try_get(kB)->x, 7.0f);
    EXPECT_FLOAT_EQ(pool_.try_get(kA)->x, 1.0f);
    EXPECT_FLOAT_EQ(pool_.try_get(kC)->x, 3.0f);
}

// Every other test here uses a trivially copyable component, where the
// std::move in erase() is indistinguishable from a copy -- so a swap-and-pop
// that accidentally copied would pass all of them. std::unique_ptr is move-only,
// so a copy would not even compile, and it satisfies all three of
// ComponentPool's static_asserts.
TEST(ComponentPoolMoveOnly, SupportsMoveOnlyComponents) {
    using Payload = std::unique_ptr<int>;
    static_assert(!std::is_copy_constructible_v<Payload>);

    ecs::ComponentPool<Payload> pool;
    ASSERT_NE(pool.try_emplace(kA, std::make_unique<int>(10)), nullptr);
    ASSERT_NE(pool.try_emplace(kB, std::make_unique<int>(20)), nullptr);
    ASSERT_NE(pool.try_emplace(kC, std::make_unique<int>(30)), nullptr);

    ASSERT_TRUE(pool.erase(kB));  // forces the move path

    ASSERT_EQ(pool.size(), 2u);
    ASSERT_NE(pool.try_get(kA), nullptr);
    ASSERT_NE(pool.try_get(kC), nullptr);
    EXPECT_EQ(**pool.try_get(kA), 10);
    EXPECT_EQ(**pool.try_get(kC), 30);
    EXPECT_EQ(pool.try_get(kB), nullptr);
}

// A heap-allocating component: the strings are deliberately longer than the
// small-string buffer, so move assignment transfers a pointer rather than
// copying bytes. Catches a swap-and-pop that leaves a moved-from husk behind.
TEST(ComponentPoolNonTrivial, SupportsHeapAllocatingComponents) {
    ecs::ComponentPool<std::string> pool;
    ASSERT_NE(pool.try_emplace(kA, std::string("alpha-past-the-small-string-buffer")), nullptr);
    ASSERT_NE(pool.try_emplace(kB, std::string("beta-past-the-small-string-buffer")), nullptr);
    ASSERT_NE(pool.try_emplace(kC, std::string("gamma-past-the-small-string-buffer")), nullptr);

    ASSERT_TRUE(pool.erase(kA));  // erase the first, moving the last into slot 0

    ASSERT_EQ(pool.size(), 2u);
    ASSERT_NE(pool.try_get(kB), nullptr);
    ASSERT_NE(pool.try_get(kC), nullptr);
    EXPECT_EQ(*pool.try_get(kB), "beta-past-the-small-string-buffer");
    EXPECT_EQ(*pool.try_get(kC), "gamma-past-the-small-string-buffer");
}
