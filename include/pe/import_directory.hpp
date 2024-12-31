#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <unordered_map>

namespace vulkan::pe
{
    class image;

    /// <summary>
    /// The abstract representation of an import directory in a PE image.
    /// </summary>
    class import_directory final
    {
        friend class image;

       public:
        /// <summary>
        /// An abstract representation of an import in the import directory.
        /// </summary>
        struct import_t
        {
            /// <summary>
            /// Creates a new import.
            /// </summary>
            /// <param name="module_name">The name of the module that the import is from.</param>
            /// <param name="import_name">The name of the import.</param>
            /// <param name="iat_rva">The relative virtual address of the import address table.</param>
            explicit import_t( const std::string_view module_name, const std::string_view import_name, std::uintptr_t iat_rva ) noexcept;

            /// <summary>
            /// The name of the module that the import is from.
            /// </summary>
            std::string module_name;

            /// <summary>
            /// The name of the import.
            /// </summary>
            std::string import_name;

            /// <summary>
            /// The relative virtual address of the import address table.
            /// </summary>
            std::uintptr_t iat_rva;
        };

       private:
        std::unordered_map< std::string, std::list< std::shared_ptr< import_t > > > _imports;

        PIMAGE_IMPORT_DESCRIPTOR _import_descriptor = nullptr;
        std::uintptr_t *_iat = nullptr;

        PIMAGE_DATA_DIRECTORY _import_data_directory = nullptr;
        PIMAGE_DATA_DIRECTORY _iat_data_directory = nullptr;

        std::size_t _import_descriptor_count = 0;

        std::size_t _import_section_size = 0;
        std::size_t _api_and_module_names_size = 0;
        std::size_t _iat_size = 0;

        /// <summary>
        /// Creates a new import directory class instance.
        /// </summary>
        explicit import_directory( ) noexcept;

        /// <summary>
        /// Refreshes the import directory.
        /// </summary>
        void refresh( image *img ) noexcept;

        /// <summary>
        /// Calculates the sizes of the import directory.
        /// </summary>
        void calculate_import_sizes( ) noexcept;

       public:
        /// <summary>
        /// Returns the IAT data directory.
        /// </summary>
        PIMAGE_DATA_DIRECTORY iat_data_directory( ) const noexcept;

        /// <summary>
        /// Gets the import data directory.
        /// </summary>
        PIMAGE_DATA_DIRECTORY import_data_directory( ) const noexcept;

        /// <summary>
        /// Returns the imports in the import directory.
        /// </summary>
        const std::list< std::shared_ptr< import_t > > imports( ) noexcept;

        /// <summary>
        /// Clears the import directory.
        /// </summary>
        void clear( ) noexcept;

        /// <summary>
        /// Adds a new import to the import directory.
        /// </summary>
        /// <param name="module_name">The name of the module that the import is from.</param>
        /// <param name="import_name">The name of the import.</param>
        /// <param name="iat_rva">The relative virtual address of the import address table.</param>
        void add( const std::string_view module_name, const std::string_view import_name, std::uintptr_t iat_rva ) noexcept;

        /// <summary>
        /// Recompiles the import directory into a new section.
        /// </summary>
        /// <param name="img">The image the import directory is associated with.</param>
        /// <param name="section_name">The name of the section to recompile to.</param>
        void recompile( image *img, const std::string_view section_name ) noexcept;
    };
}  // namespace vulkan::pe