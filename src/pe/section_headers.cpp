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