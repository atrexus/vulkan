#include "argparse/argparse.hpp"
#include "dumper.hpp"
#include "spdlog/spdlog.h"

std::stop_source stop_source;

/// <summary>
/// The console control handler. Used to terminate the application when CTRL+C or CTRL+BREAK is pressed.
/// </summary>
static BOOL WINAPI console_ctrl_handler( DWORD ctrl_type )
{
    if ( ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT )
    {
        stop_source.request_stop( );
        return TRUE;
    }

    return FALSE;
}

std::int32_t main( std::int32_t argc, char* argv[] )
{
    spdlog::set_level( spdlog::level::debug );

    argparse::ArgumentParser parser( "vulkan", "2.0.0" );

    parser.add_description(
        "A dumper for processes protected by hyperion. For best results, terminate page decryption when >50% of pages are decrypted.\nYou can "
        "terminate a task with `Ctrl+C`." );
    parser.add_epilog( "for more information, visit: https://github.com/atrexus/vulkan" );

    parser.add_argument( "-p", "--process" ).required( ).help( "the name of the process to dump" );
    parser.add_argument( "-m", "--module" ).help( "the name of the module to dump [default: \"<main-module>\"]" );
    parser.add_argument( "-o", "--output" ).help( "the name of the output file [default: \"<module>\"]" );
    parser.add_argument( "-d", "--decryption-factor" )
        .default_value< float >( 1.0f )
        .scan< 'g', float >( )
        .help( "the decryption factor to use when decrypting the PE" );
    parser.add_argument( "-i", "--resolve-imports" ).flag( ).default_value< bool >( false ).help( "rebuild the import table from scratch" );
    parser.add_argument( "-w", "--wait" ).flag( ).default_value< bool >( false ).help( "wait for the process to start" );

    // Parse the command line arguments
    try
    {
        parser.parse_args( argc, argv );
    }
    catch ( const std::exception& ex )
    {
        spdlog::error( "{}", ex.what( ) );
        return 1;
    }

    // Register the console control handler to terminate the application when CTRL+C or CTRL+BREAK is pressed.
    SetConsoleCtrlHandler( console_ctrl_handler, TRUE );

    try
    {
        std::unique_ptr< wincpp::process_t > process = nullptr;

        const auto& should_wait = parser.get< bool >( "wait" );

        do
        {
            process = wincpp::process_t::open( parser.get< std::string >( "process" ) );

            if ( should_wait )
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

        } while ( !process && should_wait );

        if ( !process )
        {
            spdlog::error( "Failed to open process" );
            return 1;
        }

        auto opts = vulkan::dumper::options::default_value( );

        if ( const auto& m = parser.present< std::string >( "-m" ) )
            opts.module_name( m.value( ) );
        else
            opts.module_name( process->name( ) );

        opts.target_decryption_factor( parser.get< float >( "decryption-factor" ) );
        opts.resolve_imports( parser.get< bool >( "resolve-imports" ) );

        const auto& image = vulkan::dumper::dump( process, opts, stop_source.get_token( ) );

        const auto& output = parser.present< std::string >( "-o" ).value_or( opts.module_name( ).data( ) );

        spdlog::info( "Dumping module: \"{}\" to \"{}\"", opts.module_name( ), output );

        image->save_to_file( output );
    }
    catch ( const std::exception& ex )
    {
        spdlog::error( "An error occurred: {}", ex.what( ) );
        return 1;
    }

    return 0;
}