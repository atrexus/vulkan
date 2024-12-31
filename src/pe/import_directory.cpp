#include "pe/import_directory.hpp"

#include <spdlog/spdlog.h>

#include "pe/image.hpp"
#include "pe/util.hpp"

namespace vulkan::pe
{
    import_directory::import_directory( ) noexcept
    {
    }

    void import_directory::refresh( image* img ) noexcept
    {
        _import_data_directory = img->data_directory( IMAGE_DIRECTORY_ENTRY_IMPORT );
        _iat_data_directory = img->data_directory( IMAGE_DIRECTORY_ENTRY_IAT );

        _import_descriptor =
            reinterpret_cast< PIMAGE_IMPORT_DESCRIPTOR >( img->buffer( ).data( ) + img->rva_to_offset( _import_data_directory->VirtualAddress ) );

        _iat = reinterpret_cast< std::uintptr_t* >( img->buffer( ).data( ) + img->rva_to_offset( _iat_data_directory->VirtualAddress ) );

        // Parse the import directory
        while ( _import_descriptor->Name )
        {
            // Get the module name
            const auto& module_name = reinterpret_cast< const char* >( img->buffer( ).data( ) + img->rva_to_offset( _import_descriptor->Name ) );

            // Get the import lookup table
            const auto& lookup_table =
                reinterpret_cast< PIMAGE_THUNK_DATA >( img->buffer( ).data( ) + img->rva_to_offset( _import_descriptor->OriginalFirstThunk ) );

            // Get the import address table
            const auto& address_table =
                reinterpret_cast< PIMAGE_THUNK_DATA >( img->buffer( ).data( ) + img->rva_to_offset( _import_descriptor->FirstThunk ) );

            // Iterate over both the lookup and address tables
            for ( std::size_t i = 0; lookup_table[ i ].u1.AddressOfData; ++i )
            {
                // Get the import name
                const auto& import_name =
                    reinterpret_cast< PIMAGE_IMPORT_BY_NAME >( img->buffer( ).data( ) + img->rva_to_offset( lookup_table[ i ].u1.AddressOfData ) )
                        ->Name;

                // Get the IAT RVA
                const auto& iat_rva = static_cast< std::uintptr_t >( _import_descriptor->FirstThunk + ( i * sizeof( std::uintptr_t ) ) );

                // Add the import to the list
                add( module_name, import_name, iat_rva );
            }

            _import_descriptor++;
        }
    }

    void import_directory::calculate_import_sizes( ) noexcept
    {
        _import_section_size = 0;
        _api_and_module_names_size = 0;
        _iat_size = 0;

        // Calculate the sizes of the import directory (add an extra import descriptor for the null terminator)
        _import_descriptor_count = _imports.size( ) + 1;

        for ( const auto& [ module_name, imports ] : _imports )
        {
            // Add the size of the module name
            _api_and_module_names_size += module_name.size( ) + 1;

            for ( const auto& import : imports )
            {
                // Add the size of the import name
                _api_and_module_names_size += sizeof( IMAGE_IMPORT_BY_NAME );
                _api_and_module_names_size += import->import_name.size( ) + 1;

                // Add the import lookup table entry
                _api_and_module_names_size += sizeof( std::uintptr_t );

                // Add the entry size
                _iat_size += sizeof( std::uintptr_t );
            }

            // Add the size of the null terminator
            _api_and_module_names_size += sizeof( std::uintptr_t ) * 2;

            // Add the size of the null terminator
            _iat_size += sizeof( std::uintptr_t );
        }

        // Calculate the size of the import directory
        _import_section_size = _iat_size + _api_and_module_names_size + ( sizeof( IMAGE_IMPORT_DESCRIPTOR ) * _import_descriptor_count );
    }

    PIMAGE_DATA_DIRECTORY import_directory::iat_data_directory( ) const noexcept
    {
        return _iat_data_directory;
    }

    PIMAGE_DATA_DIRECTORY import_directory::import_data_directory( ) const noexcept
    {
        return _import_data_directory;
    }

    const std::list< std::shared_ptr< import_directory::import_t > > import_directory::imports( ) noexcept
    {
        std::list< std::shared_ptr< import_t > > result;

        for ( const auto& [ _, imports ] : _imports )
        {
            for ( const auto& import : imports )
            {
                result.push_back( import );
            }
        }

        return result;
    }

    void import_directory::clear( ) noexcept
    {
        _imports.clear( );
    }

    void import_directory::add( const std::string_view module_name, const std::string_view import_name, std::uintptr_t iat_rva ) noexcept
    {
        auto& import_list = _imports[ module_name.data( ) ];

        // Check if the import already exists
        for ( const auto& import : import_list )
        {
            if ( import->import_name == import_name )
                return;
        }

        import_list.emplace_back( new import_t{ module_name, import_name, iat_rva } );

        // Update the sizes
        calculate_import_sizes( );
    }

    void import_directory::recompile( image* img, const std::string_view section_name ) noexcept
    {
        // Create a new section that will hold the new import directory
        const auto& section = img->append_section( section_name, IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ, _import_section_size );

        // Get the section data
        auto data = img->buffer( ).data( ) + section->PointerToRawData;

        // Set the offsets
        std::size_t iat_offset = 0, offset = _iat_size + _import_descriptor_count * sizeof( IMAGE_IMPORT_DESCRIPTOR );

        // Get the first import descriptor
        auto import_descriptor = reinterpret_cast< PIMAGE_IMPORT_DESCRIPTOR >( data + _iat_size );

        // Iterate over the import pools
        for ( const auto& [ module_name, imports ] : _imports )
        {
            // Set the pointer to the IAT for the current module
            import_descriptor->FirstThunk = section->VirtualAddress + iat_offset;

            // Set the pointer to the lookup table for the current module
            import_descriptor->OriginalFirstThunk = section->VirtualAddress + offset;

            // Get the lookup table for the current module
            auto lookup_table = reinterpret_cast< PIMAGE_THUNK_DATA >( data + offset );

            // Update the offset for the lookup table
            offset += sizeof( IMAGE_THUNK_DATA ) * ( imports.size( ) + 2 );

            // Iterate over the imports in the pool
            for ( const auto& import : imports )
            {
                // Add the IAT entry for the import
                *reinterpret_cast< std::uintptr_t* >( data + iat_offset ) = import->iat_rva;

                // Add the import by name structure
                auto import_by_name = reinterpret_cast< PIMAGE_IMPORT_BY_NAME >( data + offset );

                // Set the hint to 0
                import_by_name->Hint = 0;

                // Set the name of the import
                std::copy( import->import_name.begin( ), import->import_name.end( ), import_by_name->Name );

                // Set the offset for the lookup table
                lookup_table->u1.AddressOfData = section->VirtualAddress + offset;

                // Update the IAT offset
                iat_offset += sizeof( std::uintptr_t );

                // Update the offset for the import by name structure
                offset += sizeof( IMAGE_IMPORT_BY_NAME ) + import->import_name.size( );

                // Update the lookup table
                lookup_table++;
            }

            // Set the name of the import descriptor
            import_descriptor->Name = section->VirtualAddress + offset;

            // Write the module name
            std::copy( module_name.begin( ), module_name.end( ), data + offset );

            // Update the offset for the module name
            offset += module_name.size( ) + 2;

            // Update the IAT offset for the null terminator
            iat_offset += sizeof( std::uintptr_t );

            // Increment the import descriptor
            import_descriptor++;
        }

        // Update the data directories
        _iat_data_directory->VirtualAddress = section->VirtualAddress;
        _iat_data_directory->Size = _iat_size;

        _import_data_directory->VirtualAddress = _iat_data_directory->VirtualAddress + _iat_data_directory->Size;
        _import_data_directory->Size = _import_descriptor_count * sizeof( IMAGE_IMPORT_DESCRIPTOR );
    }

    import_directory::import_t::import_t( const std::string_view module_name, const std::string_view import_name, std::uintptr_t iat_rva ) noexcept
    {
        this->module_name = module_name;
        this->import_name = import_name;
        this->iat_rva = iat_rva;
    }
}  // namespace vulkan::pe