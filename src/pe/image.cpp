#include "pe/image.hpp"

#include <fstream>

#include "pe/util.hpp"

namespace vulkan::pe
{
    std::uint32_t image::compute_checksum( ) const noexcept
    {
        std::uint32_t sum = 0;

        const auto data = reinterpret_cast< std::uint32_t* >( _buffer.data( ) );
        const auto size = _buffer.size( ) / sizeof( std::uint32_t );

        for ( std::size_t i = 0; i < size; ++i )
        {
            if ( ( sum += data[ i ] ) > 0xFFFF )
                sum = ( sum & 0xFFFF ) + ( sum >> 0x10 );
        }

        if ( size % sizeof( std::uint32_t ) )
        {
            if ( ( sum += ( static_cast< std::uint16_t >( data[ size - 1 ] ) << 0x8 ) ) > 0xFFFF )
                sum = ( sum & 0xFFFF ) + ( sum >> 0x10 );
        }

        return ~sum;
    }

    image::image( const wincpp::modules::module_t& module )
    {
        _buffer.resize( module.size( ) );

        // The header is the first region in the module.
        const auto& header = *module.regions( ).begin( );
        const auto header_size = header.size( );

        header.read( _buffer.data( ) );

        // Create the import directory.
        _import_directory = std::unique_ptr< pe::import_directory >( new pe::import_directory( ) );

        _is_valid = refresh( );
    }

    std::vector< std::uint8_t >& image::buffer( ) const noexcept
    {
        return _buffer;
    }

    std::unique_ptr< section_headers >& image::section_headers( ) const noexcept
    {
        return _section_headers;
    }

    std::unique_ptr< import_directory >& image::import_directory( ) const noexcept
    {
        return _import_directory;
    }

    PIMAGE_DATA_DIRECTORY image::data_directory( std::uint32_t id ) const noexcept
    {
        return &_nt_headers->OptionalHeader.DataDirectory[ id ];
    }

    PIMAGE_SECTION_HEADER image::append_section( const std::string_view name, std::uint32_t characteristics, const std::span< std::uint8_t >& data )
    {
        const auto file_alignment = _nt_headers->OptionalHeader.FileAlignment;
        const auto section_alignment = _nt_headers->OptionalHeader.SectionAlignment;

        if ( file_alignment == 0 || section_alignment == 0 )
            return nullptr;

        const auto& aligned_file_size = align< std::uint32_t >( static_cast< std::uint32_t >( data.size( ) ), file_alignment );

        // Create a new section header.
        IMAGE_SECTION_HEADER section_header = { };

        // Copy the name into the section header.
        std::copy( name.begin( ), name.end( ), section_header.Name );

        // Set the virtual size to the size of the data.
        section_header.SizeOfRawData = static_cast< std::uint32_t >( aligned_file_size + data.size( ) );
        section_header.Misc.VirtualSize = static_cast< std::uint32_t >( data.size( ) );

        // Set the characteristics of the section.
        section_header.Characteristics = characteristics;

        const auto& last_section = _section_headers->last( );

        // Set the virtual address and pointer to raw data.
        section_header.VirtualAddress = align( last_section->VirtualAddress + last_section->Misc.VirtualSize, section_alignment );
        section_header.PointerToRawData = align( last_section->PointerToRawData + last_section->SizeOfRawData, file_alignment );

        // Append the section to the raw section headers.
        section_headers( )->append( section_header );

        // Update the image size.
        _nt_headers->OptionalHeader.SizeOfImage += aligned_file_size;

        // Update the NT headers.
        _nt_headers->FileHeader.NumberOfSections += 1;
        _nt_headers->OptionalHeader.SizeOfHeaders += sizeof( section_header );
        _nt_headers->OptionalHeader.SizeOfCode += static_cast< std::uint32_t >( data.size( ) );

        // Insert the data into the buffer.
        _buffer.insert( _buffer.begin( ) + section_header.PointerToRawData, data.begin( ), data.end( ) );

        if ( _is_valid = refresh( ) )
            return _section_headers->last( );

        return nullptr;
    }

    PIMAGE_SECTION_HEADER image::append_section( const std::string_view name, std::uint32_t characteristics, std::uint32_t size )
    {
        std::vector< std::uint8_t > data( size, 0x0 );

        return append_section( name, characteristics, data );
    }

    PIMAGE_SECTION_HEADER image::extend_section( const std::string_view name, std::uint32_t size )
    {
        // Find the section header.
        const auto section = _section_headers->find( name.data( ) );

        if ( !section )
            return nullptr;

        // Get file and section alignment.
        const auto file_alignment = _nt_headers->OptionalHeader.FileAlignment;
        const auto section_alignment = _nt_headers->OptionalHeader.SectionAlignment;

        if ( file_alignment == 0 || section_alignment == 0 )
            return nullptr;

        const auto& old_size = section->SizeOfRawData;

        // Update size of raw data.
        section->SizeOfRawData = align( section->SizeOfRawData + size, file_alignment );

        // Update virtual size.
        section->Misc.VirtualSize = align( section->Misc.VirtualSize + size, section_alignment );

        // Update the image size.
        _nt_headers->OptionalHeader.SizeOfImage = align( _nt_headers->OptionalHeader.SizeOfImage + size, section_alignment );

        std::unique_ptr< std::uint8_t[] > data( new std::uint8_t[ size ] );

        // Insert the data into the buffer.
        _buffer.insert( _buffer.begin( ) + section->PointerToRawData + old_size, data.get( ), data.get( ) + size );

        if ( _is_valid = refresh( ) )
            return section;

        return nullptr;
    }

    bool image::refresh( ) noexcept
    {
        _dos_header = reinterpret_cast< PIMAGE_DOS_HEADER >( _buffer.data( ) );

        if ( _dos_header->e_magic != IMAGE_DOS_SIGNATURE )
            return false;

        _nt_headers = reinterpret_cast< PIMAGE_NT_HEADERS >( _buffer.data( ) + _dos_header->e_lfanew );

        if ( !_nt_headers || _nt_headers->Signature != IMAGE_NT_SIGNATURE )
            return false;

        _section_headers.reset( new pe::section_headers( _nt_headers ) );

        if ( _nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
            return false;

        _import_directory->refresh( this );

        _nt_headers->OptionalHeader.CheckSum = compute_checksum( );

        return true;
    }

    std::uint32_t image::rva_to_offset( std::uint32_t rva ) const noexcept
    {
        for ( std::uint16_t i = 0; i < _section_headers->count( ); ++i )
        {
            const auto section = _section_headers->at( i );

            if ( rva >= section->VirtualAddress && rva < section->VirtualAddress + section->Misc.VirtualSize )
                return section->PointerToRawData + ( rva - section->VirtualAddress );
        }

        return 0;
    }

    std::uint32_t image::offset_to_rva( std::uint32_t offset ) const noexcept
    {
        for ( std::uint16_t i = 0; i < _section_headers->count( ); ++i )
        {
            const auto section = _section_headers->at( i );

            if ( offset >= section->PointerToRawData && offset < section->PointerToRawData + section->SizeOfRawData )
                return section->VirtualAddress + ( offset - section->PointerToRawData );
        }

        return 0;
    }

    bool image::save_to_file( std::string_view filepath )
    {
        std::ofstream file( filepath.data( ), std::ios::binary );

        if ( !file.is_open( ) )
            return false;

        file.write( reinterpret_cast< const char* >( _buffer.data( ) ), _buffer.size( ) );

        file.close( );

        return true;
    }

}  // namespace vulkan::pe