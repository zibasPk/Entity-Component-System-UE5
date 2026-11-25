#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ecs/component_type_id.hpp"

namespace {

using ::testing::HasSubstr;

struct Position { float x, y; };
struct Velocity { float dx, dy; };

// Compile-time proof. A static_assert either breaks the build or produces
// nothing, so it never shows up as a named case in the CTest report -- each one
// is therefore paired with a runtime EXPECT below. The static_assert is what
// proves the expression is a constant expression (i.e. that ids cost nothing at
// run time); the EXPECT is what gives the check a name when it fails.
static_assert(ecs::type_name<double>() == "double");
static_assert(ecs::type_name<const double&>() == "double");
static_assert(ecs::component_type_id<double>() != ecs::component_type_id<float>());
static_assert(ecs::component_type_id<double>() == ecs::component_type_id<const double&>());
static_assert(ecs::fnv1a_64("a") == 0xaf63dc4c8601ec8cull);

}  // namespace


// Reference vectors from the FNV-1a 64-bit specification. These pin the hash to
// the published algorithm rather than to whatever we happen to compute: a loop
// with a wrong prime, or with the xor and multiply swapped, is still perfectly
// self-consistent and would pass every other test in this file.
TEST(ComponentTypeId, Fnv1a64MatchesPublishedVectors) {
    EXPECT_EQ(ecs::fnv1a_64(""),       0xcbf29ce484222325ull);  // the offset basis
    EXPECT_EQ(ecs::fnv1a_64("a"),      0xaf63dc4c8601ec8cull);
    EXPECT_EQ(ecs::fnv1a_64("foobar"), 0x85944171f73967e8ull);
}

TEST(ComponentTypeId, DistinctTypesGetDistinctIds) {
    EXPECT_NE(ecs::component_type_id<Position>(), ecs::component_type_id<Velocity>());
    EXPECT_NE(ecs::component_type_id<Position>(), ecs::component_type_id<float>());
    EXPECT_NE(ecs::component_type_id<int>(),      ecs::component_type_id<unsigned>());
}

// Everything funnels through std::remove_cvref_t in type_name(), so a caller
// asking for const Position& must land on the same pool as one asking for
// Position. If this breaks, get_component<const T&>() silently misses its pool.
TEST(ComponentTypeId, IdIgnoresConstAndReferenceQualifiers) {
    const ecs::ComponentTypeId plain = ecs::component_type_id<Position>();

    EXPECT_EQ(ecs::component_type_id<const Position>(),  plain);
    EXPECT_EQ(ecs::component_type_id<Position&>(),       plain);
    EXPECT_EQ(ecs::component_type_id<const Position&>(), plain);
    EXPECT_EQ(ecs::component_type_id<Position&&>(),      plain);
}

TEST(ComponentTypeId, IdIsStableAcrossCalls) {
    EXPECT_EQ(ecs::component_type_id<Position>(), ecs::component_type_id<Position>());
}

// Deliberately a substring match, not an equality one: the extracted name is
// compiler-specific. MSVC emits "struct Position" (it prints an elaborated
// type specifier for class types), while GCC and Clang emit "Position". An
// equality assertion here would pass locally and fail the moment the library is
// built with Clang for the Unreal plugin.
TEST(ComponentTypeId, TypeNameContainsTheTypesSpelling) {
    EXPECT_THAT(std::string(ecs::type_name<Position>()), HasSubstr("Position"));
    EXPECT_THAT(std::string(ecs::type_name<Velocity>()), HasSubstr("Velocity"));
    EXPECT_EQ(ecs::type_name<double>(), "double");  // builtins are spelled the same everywhere
}

// Guards the probe in detail::: the prefix/suffix lengths are measured once off
// raw_type_name<double>(), so a bad measurement would truncate every name. A
// name that still contains the surrounding boilerplate means the probe slipped.
TEST(ComponentTypeId, TypeNameHasNoLeftoverSignatureText) {
    const std::string name(ecs::type_name<Position>());

    EXPECT_THAT(name, testing::Not(HasSubstr("raw_type_name")));
    EXPECT_THAT(name, testing::Not(HasSubstr("(void)")));
    EXPECT_THAT(name, testing::Not(HasSubstr("noexcept")));
    EXPECT_FALSE(name.empty());
}
