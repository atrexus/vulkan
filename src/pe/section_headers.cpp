#include "pe/section_headers.hpp"

#include <algorithm>

#include "pe/util.hpp"

#undef max

namespace vulkan::pe
{
    section_headers::section_headers( PIMAGE_NT_HEADERS nt_headers ) noexcept : _nt_headers( nt_headers )
    {
        realign( );
    }

    void section_headers::realign( ) const noexcept
    {
        const auto section_alignment = _nt_headers->OptionalHeader.SectionAlignment;
        const auto file_alignment = _nt_headers->OptionalHeader.FileAlignment;

        for ( std::uint16_t i = 0; i < count( ); ++i )
        {
            auto section = at( i );

            section->VirtualAddress = ( section->VirtualAddress / section_alignment ) * section_alignment;
            section->SizeOfRawData = ( section->SizeOfRawData / file_alignment ) * file_alignment;
        }
    }

    void section_headers::append( IMAGE_SECTION_HEADER& header ) const noexcept
    {
        auto last = at( count( ) );

        std::copy(
            reinterpret_cast< std::uint8_t* >( &header ),
            reinterpret_cast< std::uint8_t* >( &header ) + sizeof( IMAGE_SECTION_HEADER ),
            reinterpret_cast< std::uint8_t* >( last ) );

        // Update the number of sections in the NT headers.
        _nt_headers->FileHeader.NumberOfSections += 1;
    }

    void section_headers::remove( const char* name ) const noexcept
    {
        const auto num_sections = count( );

        for ( std::uint16_t i = 0; i < num_sections; ++i )
        {
            auto section = at( i );

            if ( std::strncmp( reinterpret_cast< const char* >( section->Name ), name, IMAGE_SIZEOF_SHORT_NAME ) == 0 )
            {
                remove( i );
                break;
            }
        }
    }

    void section_headers::remove( const std::uint16_t idx ) const noexcept
    {
        // Shift all later section headers one slot up to overwrite this one.
        for ( std::uint16_t j = idx + 1; j < count( ); ++j )
        {
            auto src = at( j );
            auto dst = at( j - 1 );

            std::memcpy( dst, src, sizeof( IMAGE_SECTION_HEADER ) );
        }

        // Zero out the now-redundant last section header.
        auto last = at( count( ) - 1 );
        std::memset( last, 0, sizeof( IMAGE_SECTION_HEADER ) );

        // Update section count.
        _nt_headers->FileHeader.NumberOfSections -= 1;
    }

    PIMAGE_SECTION_HEADER section_headers::find( const char* name ) const noexcept
    {
        for ( std::uint16_t i = 0; i < count( ); ++i )
        {
            const auto section = at( i );

            if ( strcmp( reinterpret_cast< const char* >( section->Name ), name ) == 0 )
                return section;
        }

        return nullptr;
    }
}  // namespace vulkan::pe