#pragma once

#include <windows.h>

#include <cstdint>

namespace vulkan::pe
{
    class image;

    /// <summary>
    /// Represents a collection of section headers in a PE image.
    /// </summary>
    class section_headers
    {
        friend class image;

        PIMAGE_NT_HEADERS _nt_headers = nullptr;

        /// <summary>
        /// Creates a new instance of the section headers class.
        /// </summary>
        /// <param name="nt_headers">The NT headers to use.</param>
        explicit section_headers( PIMAGE_NT_HEADERS nt_headers ) noexcept;

        /// <summary>
        /// Fixes the alignment of the section headers.
        /// </summary>
        void realign( ) const noexcept;

       public:
        /// <summary>
        /// Returns the number of sections in the image.
        /// </summary>
        /// <returns>The number of sections.</returns>
        constexpr std::uint16_t count( ) const noexcept
        {
            return _nt_headers->FileHeader.NumberOfSections;
        }

        /// <summary>
        /// Returns the section header at the specified index.
        /// </summary>
        /// <param name="index">The index of the section header.</param>
        /// <returns>The section header.</returns>
        constexpr PIMAGE_SECTION_HEADER at( std::uint16_t index ) const noexcept
        {
            return IMAGE_FIRST_SECTION( _nt_headers ) + index;
        }

        /// <summary>
        /// Returns the section header at the specified index.
        /// </summary>
        /// <param name="index">The index of the section header.</param>
        /// <returns>The section header.</returns>
        constexpr PIMAGE_SECTION_HEADER operator[]( std::uint16_t index ) const noexcept
        {
            return at( index );
        }

        /// <summary>
        /// Returns the last section header in the image.
        /// </summary>
        constexpr PIMAGE_SECTION_HEADER last( ) const noexcept
        {
            return at( count( ) - 1 );
        }

        /// <summary>
        /// Returns the first section header in the image.
        /// </summary>
        constexpr PIMAGE_SECTION_HEADER first( ) const noexcept
        {
            return at( 0 );
        }

        /// <summary>
        /// Appends a section header to the image.
        /// </summary>
        /// <param name="header">The section header to append.</param>
        void append( IMAGE_SECTION_HEADER& header ) const noexcept;

        /// <summary>
        /// Returns the section header with the specified name.
        /// </summary>
        /// <param name="name">The name of the section header.</param>
        /// <returns>The section header.</returns>
        PIMAGE_SECTION_HEADER find( const char* name ) const noexcept;
    };

}  // namespace vulkan::pe