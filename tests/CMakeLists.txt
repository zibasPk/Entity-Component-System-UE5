include(FetchContent)

# Pinned to a release tag, not a branch. FetchContent caches by URL, so
# "refs/heads/main.zip" gives a build that is neither reproducible (whoever
# configures first decides the version) nor updatable (the cache never
# notices main moving). URL_HASH turns a corrupted or rewritten archive into
# a build failure instead of silent input.
FetchContent_Declare(
    googletest
    URL      https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# MSVC: make googletest link the same C runtime as the rest of the build.
# Without this, googletest can pick the static CRT (/MT) while our targets
# use the dynamic one (/MD), which surfaces as LNK2038.
set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
# Keep googletest's own install rules out of this project's install set.
set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)


add_executable(ecs_tests
    registry_test.cpp
    component_type_id_test.cpp
    sparse_set_test.cpp
    entity_manager_test.cpp
    component_pool_test.cpp
)

target_link_libraries(ecs_tests PRIVATE
    ecs
    GTest::gtest_main  # googletest plus a main()
    GTest::gmock       # matchers: HasSubstr, ElementsAre, ...
)

target_compile_options(ecs_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra>
)

# Registers every TEST() with CTest individually, so `ctest` reports them
# one by one instead of as a single pass/fail for the whole binary.
include(GoogleTest)
gtest_discover_tests(ecs_tests)
