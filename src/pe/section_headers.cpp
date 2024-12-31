#include "pe/section_headers.hpp"

#include <algorithm>

namespace vulkan::pe
{
    section_headers::section_headers( PIMAGE_NT_HEADERS nt_headers ) noexcept : _nt_headers( nt_headers )
    {
    }

    void section_headers::append( IMAGE_SECTION_HEADER& header ) const noexcept
    {
        auto last = at( count( ) );

        std::copy(
            reinterpret_cast< std::uint8_t* >( &header ),
            reinterpret_cast< std::uint8_t* >( &header ) + sizeof( IMAGE_SECTION_HEADER ),
            reinterpret_cast< std::uint8_t* >( last ) );
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