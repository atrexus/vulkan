#pragma once

#include <concepts>
#include <cstdint>

namespace vulkan::pe
{
    /// <summary>
    /// Pages are aligned to 4KB on both x86 and x64.
    /// </summary>
    static constexpr std::uint32_t PAGE_SIZE = 0x1000;

    /// <summary>
    /// Aligns a value to the specified alignment.
    /// </summary>
    /// <typeparam name="T">The type of value to align.</typeparam>
    /// <param name="value">The value to align.</param>
    /// <param name="alignment">The alignment.</param>
    /// <returns>The aligned value.</returns>
    template< typename T >
    constexpr __forceinline auto align( T value, std::uint32_t alignment ) noexcept
        requires std::unsigned_integral< T >
    {
        return ( value + alignment - 1 ) & ~static_cast< T >( alignment - 1 );
    }

}  // namespace vulkan::pe