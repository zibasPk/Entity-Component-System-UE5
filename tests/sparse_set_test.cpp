#include <gtest/gtest.h>

#include "ecs/constants.hpp"
#include "ecs/sparse_set.hpp"
#include "ecs/types.hpp"

namespace {

ecs::Entity make_entity(ecs::EntityIndex index,
                        ecs::Generation generation = ecs::kFirstValidGeneration) {
    return ecs::Entity{index, generation};
}

// SparseSet is neither copyable nor movable, so it lives as a fixture member
// rather than being returned from a helper. googletest builds a fresh fixture
// for every test, so no state leaks between cases.
class SparseSetTest : public ::testing::Test {
protected:
    ecs::SparseSet set_;
};

}  // namespace


TEST_F(SparseSetTest, NewSetIsEmpty) {
    EXPECT_TRUE(set_.is_empty());
    EXPECT_EQ(set_.size(), 0u);
    EXPECT_FALSE(set_.index_of(make_entity(0)).has_value());
}

TEST_F(SparseSetTest, InsertStoresEntityAtNextDenseSlot) {
    EXPECT_TRUE(set_.insert(make_entity(7)));
    EXPECT_TRUE(set_.insert(make_entity(3)));

    EXPECT_EQ(set_.size(), 2u);
    EXPECT_FALSE(set_.is_empty());

    // Dense slots are handed out in insertion order, not by entity index.
    ASSERT_TRUE(set_.index_of(make_entity(7)).has_value());
    ASSERT_TRUE(set_.index_of(make_entity(3)).has_value());
    EXPECT_EQ(*set_.index_of(make_entity(7)), 0u);
    EXPECT_EQ(*set_.index_of(make_entity(3)), 1u);
}

TEST_F(SparseSetTest, InsertRejectsInvalidHandles) {
    EXPECT_FALSE(set_.insert(ecs::Entity{}));  // default handle: both fields invalid
    EXPECT_FALSE(set_.insert(make_entity(ecs::kInvalidIndex)));
    EXPECT_FALSE(set_.insert(make_entity(5, ecs::kInvalidGeneration)));

    EXPECT_EQ(set_.size(), 0u);
}

TEST_F(SparseSetTest, InsertRejectsIndexAtOrAboveMaxEntities) {
    EXPECT_FALSE(set_.insert(make_entity(ecs::kMaxEntities)));
    EXPECT_FALSE(set_.insert(make_entity(ecs::kMaxEntities + 1)));

    EXPECT_TRUE(set_.insert(make_entity(ecs::kMaxEntities - 1)));  // last valid slot
    EXPECT_EQ(set_.size(), 1u);
}

// Deliberate asymmetry, and the reason Registry::add_component must check
// liveness before touching a pool: insert() rejects an index that is already
// occupied WITHOUT comparing generations, while index_of() below compares the
// full handle. If this check ever became generation-aware, a stale handle could
// claim a second dense slot for an index that already owns one and desync the
// set from its parallel component vector.
TEST_F(SparseSetTest, InsertRejectsOccupiedIndexRegardlessOfGeneration) {
    ASSERT_TRUE(set_.insert(make_entity(5, 1)));

    EXPECT_FALSE(set_.insert(make_entity(5, 1)));  // same handle
    EXPECT_FALSE(set_.insert(make_entity(5, 2)));  // same index, newer generation
    EXPECT_EQ(set_.size(), 1u);
}

// The generation half of the handle is what makes stale references safe: the
// index is in bounds and occupied, but it belongs to a different entity.
TEST_F(SparseSetTest, IndexOfRejectsStaleGeneration) {
    ASSERT_TRUE(set_.insert(make_entity(5, 1)));

    EXPECT_TRUE(set_.index_of(make_entity(5, 1)).has_value());
    EXPECT_FALSE(set_.index_of(make_entity(5, 2)).has_value());
    EXPECT_FALSE(set_.index_of(make_entity(5, ecs::kInvalidGeneration)).has_value());
}

TEST_F(SparseSetTest, IsValidSparseTracksOccupancyByIndexAlone) {
    EXPECT_FALSE(set_.is_valid_sparse(5));
    EXPECT_FALSE(set_.is_valid_sparse(ecs::kMaxEntities));  // out of sparse bounds

    ASSERT_TRUE(set_.insert(make_entity(5)));
    EXPECT_TRUE(set_.is_valid_sparse(5));

    ASSERT_TRUE(set_.erase(make_entity(5)));
    EXPECT_FALSE(set_.is_valid_sparse(5));
}

TEST_F(SparseSetTest, EraseLastElementNeedsNoRemap) {
    ASSERT_TRUE(set_.insert(make_entity(0)));
    ASSERT_TRUE(set_.insert(make_entity(1)));
    ASSERT_TRUE(set_.insert(make_entity(2)));

    ASSERT_TRUE(set_.erase(make_entity(2)));  // occupies the last dense slot

    EXPECT_EQ(set_.size(), 2u);
    EXPECT_EQ(*set_.index_of(make_entity(0)), 0u);
    EXPECT_EQ(*set_.index_of(make_entity(1)), 1u);
    EXPECT_FALSE(set_.index_of(make_entity(2)).has_value());
}

// The core swap-and-pop invariant. Erasing a non-last element moves the LAST
// dense entry into the hole, so that entry's sparse mapping must be rewritten
// to point at its new slot. ComponentPool::erase performs the same move on its
// parallel vector, so if this remap is ever wrong the two containers disagree
// and a surviving entity silently reads another entity's component -- no crash,
// just wrong data.
TEST_F(SparseSetTest, EraseMiddleMovesLastEntryIntoTheHole) {
    ASSERT_TRUE(set_.insert(make_entity(0)));  // dense 0
    ASSERT_TRUE(set_.insert(make_entity(1)));  // dense 1
    ASSERT_TRUE(set_.insert(make_entity(2)));  // dense 2

    ASSERT_TRUE(set_.erase(make_entity(1)));

    EXPECT_EQ(set_.size(), 2u);
    EXPECT_FALSE(set_.index_of(make_entity(1)).has_value());
    EXPECT_EQ(*set_.index_of(make_entity(0)), 0u);  // untouched
    EXPECT_EQ(*set_.index_of(make_entity(2)), 1u);  // moved down into the hole
}

TEST_F(SparseSetTest, EraseFirstMovesLastEntryIntoTheHole) {
    ASSERT_TRUE(set_.insert(make_entity(0)));
    ASSERT_TRUE(set_.insert(make_entity(1)));
    ASSERT_TRUE(set_.insert(make_entity(2)));

    ASSERT_TRUE(set_.erase(make_entity(0)));

    EXPECT_EQ(set_.size(), 2u);
    EXPECT_FALSE(set_.index_of(make_entity(0)).has_value());
    EXPECT_EQ(*set_.index_of(make_entity(2)), 0u);
    EXPECT_EQ(*set_.index_of(make_entity(1)), 1u);
}

TEST_F(SparseSetTest, EraseReportsFailureWithoutMutating) {
    ASSERT_TRUE(set_.insert(make_entity(5, 1)));

    EXPECT_FALSE(set_.erase(make_entity(9)));                           // never inserted
    EXPECT_FALSE(set_.erase(make_entity(5, 2)));                        // stale generation
    EXPECT_FALSE(set_.erase(ecs::Entity{}));                            // invalid handle
    EXPECT_FALSE(set_.erase(make_entity(ecs::kMaxEntities)));           // out of bounds

    EXPECT_EQ(set_.size(), 1u);
    EXPECT_TRUE(set_.index_of(make_entity(5, 1)).has_value());
}

TEST_F(SparseSetTest, EraseThenReinsertReusesTheIndex) {
    ASSERT_TRUE(set_.insert(make_entity(5, 1)));
    ASSERT_TRUE(set_.erase(make_entity(5, 1)));

    // Once erased, the index is free -- including for a newer generation, which
    // is what lets a recycled entity acquire the same component type again.
    EXPECT_TRUE(set_.insert(make_entity(5, 2)));
    EXPECT_EQ(set_.size(), 1u);
    EXPECT_TRUE(set_.index_of(make_entity(5, 2)).has_value());
    EXPECT_FALSE(set_.index_of(make_entity(5, 1)).has_value());
}

TEST_F(SparseSetTest, EntitiesExposesTheDenseArrayInOrder) {
    ASSERT_TRUE(set_.insert(make_entity(7)));
    ASSERT_TRUE(set_.insert(make_entity(3)));
    ASSERT_TRUE(set_.insert(make_entity(9)));

    std::span<const ecs::Entity> live = set_.entities();
    ASSERT_EQ(live.size(), 3u);
    EXPECT_EQ(live[0].index, 7u);
    EXPECT_EQ(live[1].index, 3u);
    EXPECT_EQ(live[2].index, 9u);

    // The span must track the swap-and-pop, not a stale snapshot.
    ASSERT_TRUE(set_.erase(make_entity(3)));
    live = set_.entities();
    ASSERT_EQ(live.size(), 2u);
    EXPECT_EQ(live[0].index, 7u);
    EXPECT_EQ(live[1].index, 9u);
}
