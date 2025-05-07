#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>
#include <fstream>
#include <wincpp/process.hpp>

#include "pe/image.hpp"

namespace vulkan
{
    /// <summary>
    /// The dumper class rebuilds a PE file from a memory dump.
    /// </summary>
    class dumper final
    {
       public:
        /// <summary>
        /// The options for the dumper.
        /// </summary>
        class options final
        {
            std::string _module_name;
            float _target_decryption_factor;
            bool _resolve_imports;
            std::list< std::string > _ignore_sections;

            explicit options( ) noexcept;

           public:
            /// <summary>
            /// The default options for the dumper.
            /// </summary>
            /// <returns>A reference to the default options.</returns>
            static options default_value( ) noexcept;

            /// <summary>
            /// Gets the module name.
            /// </summary>
            std::string_view module_name( ) const noexcept;

            /// <summary>
            /// Sets the module name. This is the name of the module to dump.
            /// </summary>
            options& module_name( std::string_view value ) noexcept;

            /// <summary>
            /// Gets the target decryption factor.
            /// </summary>
            float target_decryption_factor( ) const noexcept;

            /// <summary>
            /// Sets the target decryption factor. This is the factor to use when decrypting the PE.
            /// </summary>
            options& target_decryption_factor( float factor ) noexcept;

            /// <summary>
            /// Gets whether to resolve imports.
            /// </summary>
            bool resolve_imports( ) const noexcept;

            /// <summary>
            /// Sets whether to resolve imports. This option will only work on the main module of the process.
            /// </summary>
            options& resolve_imports( bool value ) noexcept;

            /// <summary>
            /// Gets the list of sections to ignore.
            /// </summary>
            std::list< std::string >& ignore_sections( ) noexcept;

            /// <summary>
            /// Sets the list of sections to ignore. This is a list of section names to skip when dumping the PE.
            /// </summary>
            options& ignore_sections( const std::list< std::string >& sections ) noexcept;
        };

       private:
        std::unique_ptr< pe::image > _image = nullptr;

        const wincpp::modules::module_t& _module;
        std::ifstream _file;

        options _options;

        explicit dumper( const wincpp::modules::module_t& module, const options& options );

        /// <summary>
        /// Gets all imported functions from the modules.
        /// </summary>
        /// <param name="modules">The modules to get the imports from.</param>
        /// <returns>A list of exported functions used in the PE.</returns>
        std::list< std::pair< std::uintptr_t, std::shared_ptr< wincpp::modules::module_t::export_t > > > get_imports(
            const std::vector< std::shared_ptr< wincpp::modules::module_t > >& modules );

       public:
        /// <summary>
        /// Dumps the PE image from memory.
        /// </summary>
        /// <param name="process">The process to dump from.</param>
        /// <param name="options">The options for the dumper.</param>
        /// <param name="stop_token">The associated stop token.</param>
        /// <returns>A resolved PE image.</reru
        static std::unique_ptr< pe::image >
        dump( const std::unique_ptr< wincpp::process_t >& process, const dumper::options& options, std::stop_token stop_token );

        /// <summary>
        /// Resolves all of the sections in the PE file.
        /// </summary>
        /// <param name="stop_token">The associated stop token.</param>
        void resolve_sections( std::stop_token stop_token );

        /// <summary>
        /// Resolves all of the imports in the PE file.
        /// </summary>
        void resolve_imports( const std::vector< std::shared_ptr< wincpp::modules::module_t > >& modules );

        /// <summary>
        /// Walks the exception directory and makes sure that all references are valid.
        /// </summary>
        void resolve_runtime_functions( );
    };
}  // namespace vulkan