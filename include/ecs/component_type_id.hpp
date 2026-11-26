#pragma once

#include "ecs/types.hpp"
#include <string_view>
#include <type_traits>

namespace ecs {
    // fast non-cryptographic hash function trivially constexpr-able
    namespace detail {
        constexpr std::uint64_t kFNVOffsetBasis = 14695981039346656037ull;
        constexpr std::uint64_t kFNVPrime = 1099511628211ull;
    }
    constexpr ComponentTypeId fnv1a_64(std::string_view text) noexcept
    {
        ComponentTypeId hash = detail::kFNVOffsetBasis;

        for (unsigned char byte : text) {
            hash ^= byte;
            hash *= detail::kFNVPrime;

        }
        return hash;
    }
    
    //todo: move in a test file
    static_assert(fnv1a_64("a") == 0xaf63dc4c8601ec8cull);

    namespace detail { 
        template <typename T>
        constexpr std::string_view raw_type_name() noexcept
        {   
            #if defined(__clang__) || defined(__GNUC__)
                constexpr auto type_name = __PRETTY_FUNCTION__;
            #elif defined(_MSC_VER)
                constexpr auto type_name = __FUNCSIG__;
            #else
                #error "Unsupported compiler: cannot determine type name macro"
            #endif
                
            return type_name;
        }

        // code to detect the boiler plate around the type name, it should be the same for every T
        inline constexpr std::string_view kNameProbe  = raw_type_name<double>();
        inline constexpr std::string_view kProbeNeedle = "double"; // probe type must be a fundamental type — MSVC prefixes class types with struct/class

        inline constexpr std::size_t kNamePrefixLength = kNameProbe.find(kProbeNeedle);

        static_assert(kNamePrefixLength != std::string_view::npos,
            "ecs: could not locate the type name in this compiler's function signature.");

        inline constexpr std::size_t kNameSuffixLength = kNameProbe.size() - kNamePrefixLength - kProbeNeedle.size();
    }

    template <typename T>
    constexpr std::string_view type_name() noexcept 
    {
        const std::string_view raw = detail::raw_type_name<std::remove_cvref_t<T>>();
        return raw.substr(detail::kNamePrefixLength,
                      raw.size() - detail::kNamePrefixLength - detail::kNameSuffixLength);

    }

    static_assert(type_name<double>() == "double");
    static_assert(type_name<const double&>() == "double");
 
    /// @brief A component's identity is a hash of its fully qualified type name. Two types with the same name are treated as the same component.
    /// - Don't declare component types in an anonymous namespace. Types with the same name in different .cpp files would share a pool (undefined behaviour).
    /// - IDs are only stable within one compiler (MSVC, GCC and Clang format type names differently). Don't serialize them.
    /// @tparam T 
    /// @return 
    template <typename T>
    constexpr ComponentTypeId component_type_id() noexcept
    {
        std::string_view type = type_name<T>();
        auto id = fnv1a_64(type);
        return id;
    }

    //todo: move in a test file
    static_assert(component_type_id<double>() != component_type_id<float>());
    static_assert(component_type_id<double>() == component_type_id<const double&>());

}