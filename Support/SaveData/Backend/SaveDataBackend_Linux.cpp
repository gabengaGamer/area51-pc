//==============================================================================
//
//  SaveDataBackend_Linux.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SaveData/Backend/SaveDataBackend.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

//==============================================================================
//  NAMESPACE
//==============================================================================

namespace
{

namespace fs = std::filesystem;

const char* SAVE_DATA_ROOT = "SAVES";
const char* SAVE_DATA_TEMP_ROOT = "SAVES/.tmp";

//==============================================================================

save_data_status MapError( const std::error_code& Error )
{
    if( Error == std::errc::no_such_file_or_directory )
        return save_data_status::NotFound;

    if( Error == std::errc::no_space_on_device )
        return save_data_status::NoSpace;

    if( (Error == std::errc::permission_denied) ||
        (Error == std::errc::read_only_file_system) )
    {
        return save_data_status::AccessDenied;
    }

    return save_data_status::IoError;
}

//==============================================================================

fs::path MakePath( const char* pName )
{
    return fs::path( SAVE_DATA_ROOT ) / pName;
}

//==============================================================================

fs::path MakeTempPath( const char* pName )
{
    return fs::path( SAVE_DATA_TEMP_ROOT ) / (std::string( pName ) + ".tmp");
}

//==============================================================================

xbool HasTempSuffix( const std::string& Name )
{
    return (Name.size() > 4) && (Name.compare( Name.size() - 4, 4, ".tmp" ) == 0);
}

//==============================================================================

xbool IsValidFileName( const char* pName )
{
    if( (pName == NULL) || (pName[0] == '\0') )
        return FALSE;

    const std::string FileName( pName );
    const fs::path Name( pName );
    return (Name != ".") &&
           (Name != "..") &&
           (Name.filename() == Name) &&
           !Name.has_root_path() &&
           (FileName.find( '\\' ) == std::string::npos) &&
           !HasTempSuffix( FileName );
}

//==============================================================================

u64 GetFileTime( const fs::path& Path, std::error_code& Error )
{
    struct stat Info;
    if( ::stat( Path.c_str(), &Info ) != 0 )
    {
        Error = std::error_code( errno, std::generic_category() );
        return 0;
    }

    if( Info.st_mtime < 0 )
    {
        return 0;
    }

    return static_cast<u64>( Info.st_mtime ) * 1000;
}

//==============================================================================

xbool HasTempSuffix( const fs::path& Path )
{
    return HasTempSuffix( Path.filename().string() );
}

//==============================================================================

void DiscardInterruptedWrites( void )
{
    std::error_code Error;
    fs::directory_iterator It( SAVE_DATA_TEMP_ROOT, Error );
    if( Error )
        return;

    const fs::directory_iterator End;
    for( ; It != End; It.increment( Error ) )
    {
        if( Error )
            break;

        const fs::path TempPath = It->path();
        if( !It->is_regular_file( Error ) || Error || !HasTempSuffix( TempPath ) )
            continue;

        Error.clear();
        fs::remove( TempPath, Error );
        Error.clear();
    }
}

//==============================================================================

save_data_status EnsureRootDirectory( void )
{
    std::error_code Error;
    fs::create_directories( SAVE_DATA_ROOT, Error );
    if( Error )
        return MapError( Error );

    Error.clear();
    fs::create_directories( SAVE_DATA_TEMP_ROOT, Error );
    return Error ? MapError( Error ) : save_data_status::Success;
}

//==============================================================================

save_data_status CheckAvailableSpace( s32 RequiredBytes )
{
    if( RequiredBytes <= 0 )
        return save_data_status::Success;

    struct statvfs Info;
    if( statvfs( SAVE_DATA_ROOT, &Info ) != 0 )
    {
        return MapError( std::error_code( errno, std::generic_category() ) );
    }

    const std::uintmax_t Available =
        static_cast<std::uintmax_t>( Info.f_bavail ) * Info.f_frsize;
    return Available < static_cast<std::uintmax_t>( RequiredBytes )
        ? save_data_status::NoSpace
        : save_data_status::Success;
}

//==============================================================================

xbool SyncFile( const fs::path& Path )
{
    const int FileDescriptor = ::open( Path.c_str(), O_RDONLY | O_CLOEXEC );
    if( FileDescriptor < 0 )
        return FALSE;

    const int SyncResult  = ::fsync( FileDescriptor );
    const int CloseResult = ::close( FileDescriptor );
    return (SyncResult == 0) && (CloseResult == 0);
}

//==============================================================================

xbool SyncDirectory( const fs::path& Path )
{
    const int FileDescriptor = ::open( Path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC );
    if( FileDescriptor < 0 )
        return FALSE;

    const int SyncResult  = ::fsync( FileDescriptor );
    const int CloseResult = ::close( FileDescriptor );
    return (SyncResult == 0) && (CloseResult == 0);
}

} // namespace

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

save_data_status save_data_backend::Init( void )
{
    const save_data_status Status = EnsureRootDirectory();
    if( Status != save_data_status::Success )
        return Status;

    DiscardInterruptedWrites();
    return save_data_status::Success;
}

//==============================================================================

void save_data_backend::Kill( void )
{
}

//==============================================================================

save_data_status save_data_backend::List( xarray<save_data_file_info>& Files )
{
    Files.Clear();

    std::error_code Error;
    fs::directory_iterator It( SAVE_DATA_ROOT, Error );
    if( Error == std::errc::no_such_file_or_directory )
        return save_data_status::Success;
    if( Error )
        return MapError( Error );

    const fs::directory_iterator End;
    for( ; It != End; It.increment( Error ) )
    {
        if( Error )
            return MapError( Error );

        const fs::directory_entry& Entry = *It;
        if( !Entry.is_regular_file( Error ) )
        {
            if( Error )
                return MapError( Error );
            continue;
        }

        if( HasTempSuffix( Entry.path() ) )
            continue;

        const std::uintmax_t Size = Entry.file_size( Error );
        if( Error )
            return MapError( Error );
        if( Size > static_cast<std::uintmax_t>( std::numeric_limits<s32>::max() ) )
            return save_data_status::IoError;

        const u64 ModifiedTime = GetFileTime( Entry.path(), Error );
        if( Error )
            return MapError( Error );

        save_data_file_info& Info = Files.Append();
        Info.Name         = Entry.path().filename().string().c_str();
        Info.Size         = static_cast<s32>( Size );
        Info.CreationDate = ModifiedTime;
        Info.ModifiedDate = ModifiedTime;
    }

    return save_data_status::Success;
}

//==============================================================================

save_data_status save_data_backend::Read( const char* pName, xarray<u8>& Bytes )
{
    Bytes.Clear();
    if( !IsValidFileName( pName ) )
        return save_data_status::IoError;

    const fs::path Path = MakePath( pName );
    std::error_code Error;
    const std::uintmax_t Size = fs::file_size( Path, Error );
    if( Error )
        return MapError( Error );
    if( Size > static_cast<std::uintmax_t>( std::numeric_limits<s32>::max() ) )
        return save_data_status::IoError;

    std::ifstream File( Path, std::ios::binary );
    if( !File )
        return save_data_status::IoError;

    Bytes.SetCount( static_cast<s32>( Size ) );
    if( Size > 0 )
    {
        File.read( reinterpret_cast<char*>( Bytes.GetPtr() ), static_cast<std::streamsize>( Size ) );
        if( File.gcount() != static_cast<std::streamsize>( Size ) )
        {
            Bytes.Clear();
            return save_data_status::IoError;
        }
    }

    return save_data_status::Success;
}

//==============================================================================

save_data_status save_data_backend::WriteAtomic( const char* pName,
                                                 const xarray<u8>& Bytes )
{
    if( !IsValidFileName( pName ) )
        return save_data_status::IoError;

    const save_data_status RootStatus = EnsureRootDirectory();
    if( RootStatus != save_data_status::Success )
        return RootStatus;

    const save_data_status SpaceStatus = CheckAvailableSpace( Bytes.GetCount() );
    if( SpaceStatus != save_data_status::Success )
        return SpaceStatus;

    const fs::path TargetPath = MakePath( pName );
    const fs::path TempPath   = MakeTempPath( pName );

    std::ofstream File( TempPath, std::ios::binary | std::ios::trunc );
    if( !File )
        return save_data_status::IoError;

    if( Bytes.GetCount() > 0 )
    {
        File.write( reinterpret_cast<const char*>( Bytes.GetPtr() ), Bytes.GetCount() );
    }
    File.flush();
    const xbool WriteOK = File.good();
    File.close();

    if( !WriteOK )
    {
        std::error_code Error;
        fs::remove( TempPath, Error );
        return save_data_status::IoError;
    }

    if( !SyncFile( TempPath ) )
    {
        std::error_code Error;
        fs::remove( TempPath, Error );
        return save_data_status::IoError;
    }

    if( std::rename( TempPath.c_str(), TargetPath.c_str() ) != 0 )
    {
        const std::error_code Error( errno, std::generic_category() );
        std::error_code CleanupError;
        fs::remove( TempPath, CleanupError );
        return MapError( Error );
    }

    if( !SyncDirectory( SAVE_DATA_ROOT ) )
    {
        return save_data_status::IoError;
    }

    return save_data_status::Success;
}

//==============================================================================

save_data_status save_data_backend::Delete( const char* pName )
{
    if( !IsValidFileName( pName ) )
        return save_data_status::IoError;

    std::error_code Error;
    if( !fs::remove( MakePath( pName ), Error ) )
    {
        return Error ? MapError( Error ) : save_data_status::NotFound;
    }

    return save_data_status::Success;
}

//==============================================================================
