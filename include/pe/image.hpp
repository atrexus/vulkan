#pragma once

#include <span>
#include <vector>
#include <wincpp/process.hpp>

#include "pe/import_directory.hpp"
#include "pe/section_headers.hpp"

namespace vulkan::pe
{
    /// <summary>
    /// Represents a PE image.
    /// </summary>
    class image
    {
        mutable std::vector< std::uint8_t > _buffer;
        mutable std::unique_ptr< section_headers > _section_headers;
        mutable std::unique_ptr< import_directory > _import_directory;

        PIMAGE_DOS_HEADER _dos_header = nullptr;
        PIMAGE_NT_HEADERS _nt_headers = nullptr;

        bool _is_valid = false;

        /// <summary>
        /// Computes the checksum of the image.
        /// </summary>
        /// <returns>The checksum of the image.</returns>
        std::uint32_t compute_checksum( ) const noexcept;

       public:
        /// <summary>
        /// Creates a new image from a buffer.
        /// </summary>
        /// <param name="buffer">The buffer to use.</param>
        /// <param name="mapped">Whether the buffer is mapped.</param>
        explicit image( const std::vector< std::uint8_t >& buffer, bool mapped = true );

        /// <summary>
        /// Creates a new image from a loaded module.
        /// </summary>
        /// <param name="module">The module.</param>
        static std::unique_ptr< image > create( const wincpp::modules::module_t& module );

        /// <summary>
        /// Returns a reference to the internal buffer.
        /// </summary>
        std::vector< std::uint8_t >& buffer( ) const noexcept;

        /// <summary>
        /// Gets the section headers of the image.
        /// </summary>
        /// <returns>A pointer to the section headers.</returns>
        std::unique_ptr< section_headers >& section_headers( ) const noexcept;

        /// <summary>
        /// Gets the import directory of the image.
        /// </summary>
        /// <returns>A pointer to the import directory.</returns>
        std::unique_ptr< import_directory >& import_directory( ) const noexcept;

        /// <summary>
        /// Gets the data directory of the image.
        /// </summary>
        /// <param name="id">The ID of the data directory.</param>
        /// <returns>The data directory.</returns>
        PIMAGE_DATA_DIRECTORY data_directory( std::uint32_t id ) const noexcept;

        /// <summary>
        /// Adds a new section to the image.
        /// </summary>
        /// <param name="name">The name of the section.</param>
        /// <param name="characteristics">The characteristics of the section.</param>
        /// <param name="data">The data to write to the section.</param>
        /// <returns>The section header.</returns>
        PIMAGE_SECTION_HEADER append_section( const std::string_view name, std::uint32_t characteristics, const std::span< std::uint8_t >& data );

        /// <summary>
        /// Adds a new section to the image.
        /// </summary>
        /// <param name="name">The name of the section.</param>
        /// <param name="characteristics">The characteristics of the section.</param>
        /// <param name="data">The size of the section in bytes.</param>
        /// <returns>The section header.</returns>
        PIMAGE_SECTION_HEADER append_section( const std::string_view name, std::uint32_t characteristics, std::uint32_t size );

        /// <summary>
        /// Extends a section in the image.
        /// </summary>
        /// <param name="name">The name of the section.</param>
        /// <param name="size">The number of bytes to extent by (delta).</param>
        /// <returns>A pointer to the section header.</returns>
        PIMAGE_SECTION_HEADER extend_section( const std::string_view name, std::uint32_t size );

        /// <summary>
        /// Refreshes the current image and its headers.
        /// </summary>
        /// <returns>True if the image is valid, false otherwise.</returns>
        bool refresh( ) noexcept;

        /// <summary>
        /// Converts a relative virtual address to a file offset.
        /// </summary>
        /// <param name="rva">The relative virtual address.</param>
        /// <returns>The file offset.</returns>
        std::uint32_t rva_to_offset( std::uint32_t rva ) const noexcept;

        /// <summary>
        /// Converts a file offset to a relative virtual address.
        /// </summary>
        /// <param name="offset">The file offset.</param>
        /// <returns>A relative virtual address.</returns>
        std::uint32_t offset_to_rva( std::uint32_t offset ) const noexcept;

        /// <summary>
        /// Returns whether the image is valid.
        /// </summary>
        /// <returns>True if the image is valid, false otherwise.</returns>
        constexpr bool is_valid( ) const noexcept
        {
            return _is_valid;
        }

        /// <summary>
        /// Gets the base address of the image.
        /// </summary>
        constexpr std::uintptr_t image_base( ) const noexcept
        {
            return static_cast< std::uintptr_t >( _nt_headers->OptionalHeader.ImageBase );
        }

        /// <summary>
        /// Saves the image to a file.
        /// </summary>
        /// <param name="filepath">The path to save the image to.</param>
        /// <returns>True if the image was saved successfully, false otherwise.</returns>
        bool save_to_file( std::string_view filepath );

        /// <summary>
        /// Changes the base address of the image. It also parses the relocation directory and updates the image.
        /// </summary>
        /// <param name="base">The new base address.</param>
        void rebase( std::uintptr_t base ) const noexcept;
    };

}  // namespace vulkan::pe