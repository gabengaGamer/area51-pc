//==============================================================================
//
//  SaveDataBackend_Windows.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SaveData/Backend/SaveDataBackend.hpp"
#include <windows.h>

//==============================================================================
//  NAMESPACE
//==============================================================================

namespace
{
const char* SAVE_DATA_ROOT = "SAVES";
const char* SAVE_DATA_TEMP_ROOT = "SAVES\\.tmp";

//==============================================================================

xstring MakePath( const char* pName )
{
    return xstring( xfs( "%s\\%s", SAVE_DATA_ROOT, pName ) );
}

//==============================================================================

xstring MakeTempPath( const char* pName )
{
    return xstring( xfs( "%s\\%s.tmp", SAVE_DATA_TEMP_ROOT, pName ) );
}

//==============================================================================

SaveDataStatus MapError( DWORD Error )
{
    switch( Error )
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return SaveDataStatus::NotFound;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return SaveDataStatus::NoSpace;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return SaveDataStatus::AccessDenied;
    default:
        return SaveDataStatus::IoError;
    }
}

//==============================================================================

SaveDataStatus CheckAvailableSpace( s32 RequiredBytes )
{
    if( RequiredBytes <= 0 )
    {
        return SaveDataStatus::Success;
    }

    ULARGE_INTEGER Available;
    if( !GetDiskFreeSpaceExA( SAVE_DATA_ROOT, &Available, NULL, NULL ) )
    {
        return MapError( GetLastError() );
    }

    return Available.QuadPart < static_cast<u64>( RequiredBytes )
        ? SaveDataStatus::NoSpace
        : SaveDataStatus::Success;
}

//==============================================================================

u64 FileTimeToUnixMilliseconds( const FILETIME& Time )
{
    ULARGE_INTEGER Value;
    Value.LowPart  = Time.dwLowDateTime;
    Value.HighPart = Time.dwHighDateTime;

    constexpr u64 WINDOWS_TO_UNIX_EPOCH = 116444736000000000ULL;
    if( Value.QuadPart < WINDOWS_TO_UNIX_EPOCH )
    {
        return 0;
    }

    return (Value.QuadPart - WINDOWS_TO_UNIX_EPOCH) / 10000ULL;
}

//==============================================================================

xbool HasTempSuffix( const char* pName )
{
    const s32 Length = pName ? x_strlen( pName ) : 0;
    return( (Length > 4) && (x_stricmp( pName + Length - 4, ".tmp" ) == 0) );
}

//==============================================================================

xbool IsValidFileName( const char* pName )
{
    if( (pName == NULL) || (pName[0] == '\0') ||
        (x_stricmp( pName, "." ) == 0) ||
        (x_stricmp( pName, ".." ) == 0) ||
        HasTempSuffix( pName ) )
    {
        return FALSE;
    }

    for( const char* pChar = pName; *pChar; ++pChar )
    {
        if( (*pChar == '\\') || (*pChar == '/') )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

SaveDataStatus EnsureDirectory( const char* pPath )
{
    if( CreateDirectoryA( pPath, NULL ) )
    {
        return SaveDataStatus::Success;
    }

    const DWORD Error = GetLastError();
    if( Error != ERROR_ALREADY_EXISTS )
    {
        return MapError( Error );
    }

    const DWORD Attributes = GetFileAttributesA( pPath );
    return (Attributes != INVALID_FILE_ATTRIBUTES) &&
           (Attributes & FILE_ATTRIBUTE_DIRECTORY)
        ? SaveDataStatus::Success
        : SaveDataStatus::IoError;
}

//==============================================================================

void RecoverInterruptedWrites( void )
{
    WIN32_FIND_DATAA FindData;
    HANDLE Find = FindFirstFileA( (const char*)MakeTempPath( "*" ), &FindData );
    if( Find == INVALID_HANDLE_VALUE )
    {
        return;
    }

    do
    {
        if( (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            !HasTempSuffix( FindData.cFileName ) )
        {
            continue;
        }

        const xstring TempPath = xstring( xfs( "%s\\%s", SAVE_DATA_TEMP_ROOT, FindData.cFileName ) );
        DeleteFileA( TempPath );
    }
    while( FindNextFileA( Find, &FindData ) );

    FindClose( Find );
}

} // namespace

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

SaveDataStatus save_data_backend::Init( void )
{
    SaveDataStatus Status = EnsureDirectory( SAVE_DATA_ROOT );
    if( Status != SaveDataStatus::Success )
    {
        return Status;
    }

    Status = EnsureDirectory( SAVE_DATA_TEMP_ROOT );
    if( Status != SaveDataStatus::Success )
    {
        return Status;
    }

    RecoverInterruptedWrites();
    return SaveDataStatus::Success;
}

//==============================================================================

void save_data_backend::Kill( void )
{
}

//==============================================================================

SaveDataStatus save_data_backend::List( xarray<save_data_file_info>& Files )
{
    Files.Clear();

    WIN32_FIND_DATAA FindData;
    HANDLE Find = FindFirstFileA( (const char*)MakePath( "*" ), &FindData );
    if( Find == INVALID_HANDLE_VALUE )
    {
        const DWORD Error = GetLastError();
        return Error == ERROR_FILE_NOT_FOUND
            ? SaveDataStatus::Success
            : MapError( Error );
    }

    do
    {
        if( (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            HasTempSuffix( FindData.cFileName ) )
        {
            continue;
        }

        ULARGE_INTEGER Size;
        Size.LowPart  = FindData.nFileSizeLow;
        Size.HighPart = FindData.nFileSizeHigh;
        if( Size.QuadPart > 0x7fffffff )
        {
            FindClose( Find );
            return SaveDataStatus::IoError;
        }
        save_data_file_info& Info = Files.Append();        

        Info.Name         = FindData.cFileName;
        Info.Size         = static_cast<s32>( Size.QuadPart );
        Info.CreationDate = FileTimeToUnixMilliseconds( FindData.ftCreationTime );
        Info.ModifiedDate = FileTimeToUnixMilliseconds( FindData.ftLastWriteTime );
    }
    while( FindNextFileA( Find, &FindData ) );

    const DWORD Error = GetLastError();

    FindClose( Find );

    if( Error != ERROR_NO_MORE_FILES )
    {
        return MapError( Error );
    }

    return SaveDataStatus::Success;
}

//==============================================================================

SaveDataStatus save_data_backend::Read( const char* pName, xarray<u8>& Bytes )
{
    Bytes.Clear();

    if( !IsValidFileName( pName ) )
    {
        return SaveDataStatus::IoError;
    }

    const xstring Path = MakePath( pName );
    HANDLE File = CreateFileA( Path,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL );
    if( File == INVALID_HANDLE_VALUE )
    {
        return MapError( GetLastError() );
    }

    LARGE_INTEGER Size;
    if( !GetFileSizeEx( File, &Size ) )
    {
        const DWORD Error = GetLastError();
        CloseHandle( File );
        return MapError( Error );
    }
    if( (Size.QuadPart < 0) || (Size.QuadPart > 0x7fffffff) )
    {
        CloseHandle( File );
        return SaveDataStatus::IoError;
    }

    Bytes.SetCount( (s32)Size.QuadPart );
    DWORD BytesRead = 0;
    const BOOL ReadOK = (Bytes.GetCount() == 0) ||
        ReadFile( File, Bytes.GetPtr(), (DWORD)Bytes.GetCount(), &BytesRead, NULL );
    const DWORD Error = ReadOK ? ERROR_SUCCESS : GetLastError();
    CloseHandle( File );

    if( !ReadOK || (BytesRead != (DWORD)Bytes.GetCount()) )
    {
        Bytes.Clear();
        return MapError( Error );
    }
    return SaveDataStatus::Success;
}

//==============================================================================

SaveDataStatus save_data_backend::WriteAtomic( const char* pName,
                                                 const xarray<u8>& Bytes )
{
    if( !IsValidFileName( pName ) )
    {
        return SaveDataStatus::IoError;
    }

    const SaveDataStatus DirectoryStatus = EnsureDirectory( SAVE_DATA_ROOT );
    if( DirectoryStatus != SaveDataStatus::Success )
    {
        return DirectoryStatus;
    }

    const SaveDataStatus TempDirectoryStatus = EnsureDirectory( SAVE_DATA_TEMP_ROOT );
    if( TempDirectoryStatus != SaveDataStatus::Success )
    {
        return TempDirectoryStatus;
    }

    const SaveDataStatus SpaceStatus = CheckAvailableSpace( Bytes.GetCount() );
    if( SpaceStatus != SaveDataStatus::Success )
    {
        return SpaceStatus;
    }

    const xstring TargetPath = MakePath( pName );
    const xstring TempPath   = MakeTempPath( pName );

    HANDLE File = CreateFileA( TempPath,
                               GENERIC_WRITE,
                               0,
                               NULL,
                               CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL );
    if( File == INVALID_HANDLE_VALUE )
    {
        return MapError( GetLastError() );
    }

    DWORD BytesWritten = 0;
    BOOL WriteOK = (Bytes.GetCount() == 0) ||
        WriteFile( File, Bytes.GetPtr(), (DWORD)Bytes.GetCount(), &BytesWritten, NULL );
    if( WriteOK && (BytesWritten == (DWORD)Bytes.GetCount()) )
    {
        WriteOK = FlushFileBuffers( File );
    }

    DWORD Error = WriteOK ? ERROR_SUCCESS : GetLastError();
    CloseHandle( File );
    if( !WriteOK || (BytesWritten != (DWORD)Bytes.GetCount()) )
    {
        DeleteFileA( TempPath );
        return MapError( Error );
    }

    if( !MoveFileExA( TempPath,
                      TargetPath,
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) )
    {
        return MapError( GetLastError() );
    }
    return SaveDataStatus::Success;
}

//==============================================================================

SaveDataStatus save_data_backend::Delete( const char* pName )
{
    if( !IsValidFileName( pName ) )
    {
        return SaveDataStatus::IoError;
    }

    if( DeleteFileA( (const char*)MakePath( pName ) ) )
    {
        return SaveDataStatus::Success;
    }
    return MapError( GetLastError() );
}
