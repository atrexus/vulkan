#include "dumper.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <print>
#include <unordered_map>
#include <wincpp/patterns/scanner.hpp>

#undef max

namespace vulkan
{
    dumper::dumper( const wincpp::modules::module_t& module, const dumper::options& options ) : _module( module ), _options( options )
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

        return std::move( d->_image );
    }

    void dumper::resolve_sections( std::stop_token stop_token )
    {
        for ( std::size_t idx = 0; idx < _image->section_headers( )->count( ); ++idx )
        {
            const auto& header = _image->section_headers( )->at( idx );

            const auto& absolute_address = _image->image_base( ) + header->VirtualAddress;
            const auto& name = reinterpret_cast< const char* >( header->Name );

            spdlog::info( "Resolving Section: \"{}\" @ 0x{:X} - {} bytes", name, absolute_address, header->Misc.VirtualSize );

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

                while ( !stop_token.stop_requested( ) && ( pages_read.size( ) < total_pages ) )
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
                            if ( const auto& data = _module.read( absolute_address + page_rva, 0x1000 ) )
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
                if ( const auto& data = _module.read( absolute_address, header->SizeOfRawData ) )
                {
                    // Copy the data into the image buffer.
                    std::copy( data.get( ), data.get( ) + header->SizeOfRawData, _image->buffer( ).begin( ) + header->PointerToRawData );
                    continue;
                }
                else
                {
                    spdlog::error( "Failed to read section: \"{}\". Filling with zeros.", name );
                }

                // Copy zeros into the image buffer.
                std::fill(
                    _image->buffer( ).begin( ) + header->PointerToRawData,
                    _image->buffer( ).begin( ) + header->PointerToRawData + header->SizeOfRawData,
                    0x00 );
            }
        }
    }

    void dumper::resolve_imports( const std::vector< std::shared_ptr< wincpp::modules::module_t > >& modules )
    {
        _image->refresh( );

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

        spdlog::debug( "Locating cross references..." );

        struct reference_t
        {
            std::uintptr_t address;
            std::uint32_t offset;
            std::uint32_t len;
        };

        std::vector< reference_t > references;

        // Now we need to resolve all of the imports individually. We need to scan the entire image for refrences to the imports.
        const auto& calls = wincpp::patterns::scanner::find_all< wincpp::patterns::scanner::algorithm_t::naive_t >(
            _image->buffer( ), wincpp::patterns::pattern_t( "\xFF\x15\x00\x00\x00\x00", "xx????" ) );

        // Add the calls to the references vector
        for ( const auto& call : calls )
            references.push_back( { call, 2, 6 } );

        // Now we need to search for all relative jumps to the imports.
        const auto& jumps = wincpp::patterns::scanner::find_all< wincpp::patterns::scanner::algorithm_t::naive_t >(
            _image->buffer( ), wincpp::patterns::pattern_t( "\x48\xFF\x25\x00\x00\x00\x00", "xx?????" ) );

        // Add the jumps to the references vector
        for ( const auto& jump : jumps )
            references.push_back( { jump, 3, 7 } );

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
}  // namespace vulkan