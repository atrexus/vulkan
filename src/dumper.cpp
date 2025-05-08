#include "dumper.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <print>
#include <unordered_map>
#include <wincpp/patterns/scanner.hpp>

// clang-format off

#include <DbgHelp.h>
#pragma comment( lib, "Dbghelp.lib" )

// clang-format on

#undef max

namespace vulkan
{
    dumper::dumper( const wincpp::modules::module_t& module, const dumper::options& options )
        : _module( module ),
          _file( module.path( ), std::ios::binary ),
          _options( options )
    {
        spdlog::debug( "Module: \"{}\" @ 0x{:X} - {} bytes", _module.name( ), _module.address( ), _module.size( ) );

        _image.reset( new pe::image( _module ) );
    }

    std::list< std::pair< std::uintptr_t, std::shared_ptr< wincpp::modules::module_t::export_t > > > dumper::get_imports(
        const std::vector< std::shared_ptr< wincpp::modules::module_t > >& modules )
    {
        std::list< std::pair< std::uintptr_t, std::shared_ptr< wincpp::modules::module_t::export_t > > > imports;

        // Export map for resolving addresses
        std::unordered_map< std::uintptr_t, std::shared_ptr< wincpp::modules::module_t::export_t > > export_map;

        // Populate the export map
        for ( const auto& module : modules )
        {
            for ( const auto& e : module->exports( ) )
            {
                export_map[ e->address( ) ] = e;
            }
        }

        // Get the .rdata section
        if ( const auto& rdata = _module.fetch_section( ".rdata" ) )
        {
            // Read the entire section
            const auto& buffer = rdata->read( );

            // Iterate over each address in the section
            for ( std::size_t i = 0; i < rdata->size( ); ++i )
            {
                const auto& address = *reinterpret_cast< std::uintptr_t* >( buffer.get( ) + i );

                if ( !address )
                    continue;

                // Check if the address is in the export map
                if ( const auto& e = export_map.find( address ); e != export_map.end( ) )
                    imports.push_back( { address, e->second } );
            }
        }

        return imports;
    }

    std::unique_ptr< pe::image >
    dumper::dump( const std::unique_ptr< wincpp::process_t >& process, const dumper::options& options, std::stop_token stop_token )
    {
        const auto& m = process->module_factory[ options.module_name( ) ];

        std::unique_ptr< dumper > d( new dumper( m, options ) );

        d->resolve_sections( stop_token );

        if ( options.resolve_imports( ) )
            d->resolve_imports( process->module_factory.modules( ) );

        d->resolve_runtime_functions( );

        if ( options.image_base( ) != -1 )
        {
            spdlog::info( "Rebasing image to 0x{:X}", options.image_base( ) );

            d->_image->rebase( options.image_base( ) );
        }

        if ( !options.minidump_path( ).empty( ) )
        {
            spdlog::info( "Creating minidump at \"{}\"", options.minidump_path( ) );

            // Create a minidump of the process.
            d->save_minidump( process, options.minidump_path( ) );
        }

        // Refresh the image one last time. This will recalculate the checksum.
        d->_image->refresh( );

        return std::move( d->_image );
    }

    void dumper::resolve_sections( std::stop_token stop_token )
    {
        for ( std::size_t idx = 0; idx < _image->section_headers( )->count( ); ++idx )
        {
            const auto& header = _image->section_headers( )->at( idx );

            // Skip invalid sections
            if ( !header->PointerToRawData || !header->SizeOfRawData )
                continue;

            const auto& name = reinterpret_cast< const char* >( header->Name );

            // Skip sections that are explicitly ignored.
            if ( std::find( _options.ignore_sections( ).begin( ), _options.ignore_sections( ).end( ), name ) != _options.ignore_sections( ).end( ) )
            {
                spdlog::debug( "Ignoring section: \"{}\"", name );

                _image->section_headers( )->remove( idx );
                --idx;
                continue;
            }

            const auto& absolute_address = _image->image_base( ) + header->VirtualAddress;

            spdlog::info( "Resolving section: \"{}\" @ 0x{:X} - {} bytes", name, absolute_address, header->Misc.VirtualSize );

            // We need to read code sections page by page.
            if ( header->Characteristics & IMAGE_SCN_CNT_CODE )
            {
                std::unordered_set< std::uintptr_t > pages_read;
                const auto total_pages = header->Misc.VirtualSize / 0x1000;

                // Before we do anything, fill the buffer with nop instructions.
                std::fill(
                    _image->buffer( ).begin( ) + header->PointerToRawData,
                    _image->buffer( ).begin( ) + header->PointerToRawData + header->SizeOfRawData,
                    0x90 );

                while ( !stop_token.stop_requested( ) && ( pages_read.size( ) <= total_pages ) )
                {
                    for ( auto page = 0; page < total_pages; ++page )
                    {
                        const auto page_rva = page * 0x1000;

                        // If we've read this page, skip.
                        if ( pages_read.find( page ) != pages_read.end( ) )
                            continue;

                        const auto& region = *_module.factory[ absolute_address + page_rva ].regions( ).begin( );
                        const auto offset = header->PointerToRawData + page_rva;

                        // If the page is not accessible, skip.
                        if ( !region.protection( ).has( wincpp::memory::protection_t::noaccess_t ) )
                        {
                            if ( const auto& data = _module.factory.read( absolute_address + page_rva, 0x1000 ) )
                            {
                                const auto percent = static_cast< double >( pages_read.size( ) ) / total_pages * 100.0;

                                spdlog::debug(
                                    "Read page @ 0x{:X} ({}/{}) = {:.3f}%", absolute_address + page_rva, pages_read.size( ), total_pages, percent );

                                // Copy the data into the image buffer.
                                std::copy( data.get( ), data.get( ) + 0x1000, _image->buffer( ).begin( ) + offset );

                                // Mark the page as read.
                                pages_read.insert( page );
                                continue;
                            }
                        }
                    }
                }
            }
            else
            {
                if ( const auto& data = _module.factory.read( absolute_address, header->SizeOfRawData ) )
                {
                    // Copy the data into the image buffer.
                    std::copy( data.get( ), data.get( ) + header->SizeOfRawData, _image->buffer( ).begin( ) + header->PointerToRawData );
                    continue;
                }

                if ( _file )
                {
                    const auto relocation_directory = _image->data_directory( IMAGE_DIRECTORY_ENTRY_BASERELOC );

                    // Check if the section corresponds to the `.reloc` section. Often times discarded sections are not readable, so we'll copy their
                    // contents from the image backed by the disk.
                    if ( relocation_directory->VirtualAddress == header->VirtualAddress && relocation_directory->Size == header->Misc.VirtualSize )
                    {
                        // Section is always mapped, so no need to check for bounds.
                        _file.seekg( header->PointerToRawData, std::ios::beg );

                        std::vector< std::uint8_t > buffer( header->SizeOfRawData );

                        // Read the section from the file.
                        if ( _file.read( reinterpret_cast< char* >( buffer.data( ) ), header->SizeOfRawData ) )
                        {
                            // Copy the data into the image buffer.
                            std::copy( buffer.begin( ), buffer.end( ), _image->buffer( ).begin( ) + header->PointerToRawData );
                            continue;
                        }
                    }
                }

                spdlog::error( "Failed to read section: \"{}\". Filling with zeros.", name );

                // Copy zeros into the image buffer.
                std::fill(
                    _image->buffer( ).begin( ) + header->PointerToRawData,
                    _image->buffer( ).begin( ) + header->PointerToRawData + header->SizeOfRawData,
                    0x00 );
            }
        }

        spdlog::debug( "Resolved all sections" );
    }

    void dumper::resolve_imports( const std::vector< std::shared_ptr< wincpp::modules::module_t > >& modules )
    {
        spdlog::info( "Resolving import directory: \".vulkan\"" );

        _image->refresh( );

        spdlog::debug( "Collecting all exported functions" );

        // Get the imports from the modules
        const auto& imports = get_imports( modules );

        // Build the import pools
        for ( const auto& [ address, imp ] : imports )
        {
            // Add the import to the IAT
            _image->import_directory( )->add( imp->module( )->name( ), imp->name( ), address );
        }

        _image->import_directory( )->recompile( _image.get( ), ".vulkan" );

        // Refresh the image.
        _image->import_directory( )->clear( );
        _image->refresh( );

        // Now we create a map that maps the value of the IAT entries to their IAT entry rva.
        std::unordered_map< std::uintptr_t, std::uintptr_t > iat_map;

        // Iterate over the imports and add them to the map.
        for ( const auto& import : _image->import_directory( )->imports( ) )
        {
            // Read the IAT entry
            const auto& iat_entry = *reinterpret_cast< std::uintptr_t* >( _image->buffer( ).data( ) + _image->rva_to_offset( import->iat_rva ) );

            // Add the IAT entry to the map
            iat_map[ iat_entry ] = import->iat_rva;
        }

        spdlog::debug( "Searching for references to the exported routines" );

        struct reference_t
        {
            std::uintptr_t address;
            std::uint32_t offset;
            std::uint32_t len;
        };

        // An immutable list of patterns to search for. The first parameter is the pattern, the second is the offset and length of the relative
        // address. The `address` field is always set to zero, as it gets filled in later.
        const std::list< std::pair< wincpp::patterns::pattern_t, reference_t > > patterns = {
            { wincpp::patterns::pattern_t( "\xFF\x15\x00\x00\x00\x00", "xx????" ), { 0, 2, 6 } },
            { wincpp::patterns::pattern_t( "\x48\xFF\x25\x00\x00\x00\x00", "xx?????" ), { 0, 3, 7 } }
        };

        std::vector< reference_t > references;

        for ( const auto& [ pattern, reference ] : patterns )
        {
            const auto& results =
                wincpp::patterns::scanner::find_all< wincpp::patterns::scanner::algorithm_t::naive_t >( _image->buffer( ), pattern );

            for ( const auto& result : results )
                references.push_back( { result, reference.offset, reference.len } );
        }

        spdlog::debug( "Processing {} cross references", references.size( ) );

        for ( const auto& reference : references )
        {
            // Extract the relative offset from the instruction
            auto offset = reinterpret_cast< std::uint32_t* >( _image->buffer( ).data( ) + reference.address + reference.offset );

            // Compute the absolute address of the call instruction
            const auto next_instruction = reference.address + reference.len;
            const auto& absolute_address = next_instruction + *offset;

            // Dereference the absolute address
            const auto& export_address =
                *reinterpret_cast< std::uintptr_t* >( _image->buffer( ).data( ) + _image->rva_to_offset( absolute_address ) );

            // Quick check to see if the dereferenced address could be a code address
            if ( export_address < 0x00007FF000000000 || export_address > 0x00007FFFFFFFFFFF )
                continue;

            // Check if the old IAT entry is in the IAT map
            if ( const auto& iat_entry = iat_map.find( export_address ); iat_entry != iat_map.end( ) )
            {
                // Get the new IAT entry RVA
                const auto& new_iat_rva = iat_entry->second;

                // Compute the new relative offset
                const auto& new_offset = new_iat_rva - next_instruction;

                // Write the new relative offset
                *offset = new_offset;

                spdlog::debug( "Patched instruction @ 0x{:X} to 0x{:X}", _image->image_base( ) + reference.address, *offset );
            }
        }
    }

    void dumper::resolve_runtime_functions( )
    {
        const auto exception_directory = _image->data_directory( IMAGE_DIRECTORY_ENTRY_EXCEPTION );

        if ( !exception_directory->VirtualAddress || !exception_directory->Size )
            return;

        for ( auto rva = exception_directory->VirtualAddress; rva < exception_directory->VirtualAddress + exception_directory->Size;
              rva += sizeof( IMAGE_RUNTIME_FUNCTION_ENTRY ) )
        {
            const auto& entry = *reinterpret_cast< PIMAGE_RUNTIME_FUNCTION_ENTRY >( _image->buffer( ).data( ) + _image->rva_to_offset( rva ) );

            const auto unwind_offset = _image->rva_to_offset( entry.UnwindInfoAddress );

            // Check if the entry is valid
            if ( _image->rva_to_offset( entry.BeginAddress ) && _image->rva_to_offset( entry.EndAddress ) && unwind_offset )
                continue;

            struct unwind_info_t
            {
                std::uint8_t version : 3;
                std::uint8_t flags : 5;

                // No need to implement the rest, as we don't need it.
            };

            const auto unwind_info = *reinterpret_cast< unwind_info_t* >( _image->buffer( ).data( ) + unwind_offset );

            // Check if the unwind info is valid
            if ( unwind_info.version == 1 )
                continue;

            spdlog::warn( "Invalid runtime function entry @ 0x{:X}. Removing.", rva );

            const auto offset = _image->rva_to_offset( rva );

            // Remove the entry from the image;
            std::fill( _image->buffer( ).begin( ) + offset, _image->buffer( ).begin( ) + offset + sizeof( IMAGE_RUNTIME_FUNCTION_ENTRY ), 0x00 );
        }
    }

    void dumper::save_minidump( const std::unique_ptr< wincpp::process_t >& process, const std::string_view path ) const
    {
        const auto& handle =
            wincpp::core::handle_t::create( CreateFileA( path.data( ), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr ) );

        if ( handle->native == INVALID_HANDLE_VALUE )
            throw wincpp::core::error::from_win32( GetLastError( ) );

        const auto minidump_type = static_cast< MINIDUMP_TYPE >(
            MiniDumpWithFullMemoryInfo | MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo | MiniDumpWithModuleHeaders );

        if ( !MiniDumpWriteDump( process->handle->native, process->id( ), handle->native, minidump_type, nullptr, nullptr, nullptr ) )
            throw wincpp::core::error::from_win32( GetLastError( ) );
    }

    dumper::options::options( ) noexcept : _module_name( ), _target_decryption_factor( 1.0f ), _resolve_imports( false )
    {
    }

    dumper::options dumper::options::default_value( ) noexcept
    {
        return options( );
    }

    std::string_view dumper::options::module_name( ) const noexcept
    {
        return _module_name;
    }

    dumper::options& dumper::options::module_name( std::string_view value ) noexcept
    {
        _module_name = std::string( value );
        return *this;
    }

    float dumper::options::target_decryption_factor( ) const noexcept
    {
        return _target_decryption_factor;
    }

    dumper::options& dumper::options::target_decryption_factor( float factor ) noexcept
    {
        _target_decryption_factor = factor;
        return *this;
    }

    bool dumper::options::resolve_imports( ) const noexcept
    {
        return _resolve_imports;
    }

    dumper::options& dumper::options::resolve_imports( bool value ) noexcept
    {
        _resolve_imports = value;
        return *this;
    }

    std::list< std::string >& dumper::options::ignore_sections( ) noexcept
    {
        return _ignore_sections;
    }

    dumper::options& dumper::options::ignore_sections( const std::list< std::string >& sections ) noexcept
    {
        _ignore_sections = sections;
        return *this;
    }
    std::uintptr_t dumper::options::image_base( ) const noexcept
    {
        return _image_base;
    }

    dumper::options& dumper::options::image_base( std::uintptr_t base ) noexcept
    {
        _image_base = base;
        return *this;
    }

    std::string_view dumper::options::minidump_path( ) const noexcept
    {
        return _minidump_path;
    }

    dumper::options& dumper::options::minidump_path( std::string_view path ) noexcept
    {
        _minidump_path = std::string( path );
        return *this;
    }
}  // namespace vulkan