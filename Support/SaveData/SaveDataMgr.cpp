//==============================================================================
//
//  SaveDataMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SaveDataMgr.hpp"
#include "Backend/SaveDataBackend.hpp"
#include "SaveDataCodec.hpp"
#include "StateMgr/StateMgr.hpp"
#include "x_files/x_workers.hpp"

#include <algorithm>
#include <deque>
#include <vector>

//==============================================================================
//  NAMESPACE
//==============================================================================

namespace
{

const char* SETTINGS_FILE_NAME             = "AREA-51-SETTINGS";
const char* PROFILE_FILE_PREFIX            = "AREA-51-PROFILE";
const s32   SAVE_DATA_PROFILE_CAPACITY     = 32;
const s32   SAVE_DATA_MAX_PENDING_REQUESTS = 16;

//==============================================================================

xstring GetProfileFileName( s32 ProfileID )
{
    return xstring( xfs( "%s%05d", PROFILE_FILE_PREFIX, ProfileID ) );
}

//==============================================================================

xbool ParseProfileID( const char* pName, s32& ProfileID )
{
    const s32 PrefixLength = x_strlen( PROFILE_FILE_PREFIX );
    if( !pName ||
        (x_strlen( pName ) != PrefixLength + 5) ||
        (x_strncmp( pName, PROFILE_FILE_PREFIX, PrefixLength ) != 0) )
    {
        return FALSE;
    }

    s32 Value = 0;
    for( s32 i = 0; i < 5; i++ )
    {
        const char Digit = pName[PrefixLength + i];
        if( (Digit < '0') || (Digit > '9') )
        {
            return FALSE;
        }
        Value = Value * 10 + Digit - '0';
    }

    if( (Value < 0) || (Value >= SAVE_DATA_PROFILE_CAPACITY) )
    {
        return FALSE;
    }

    ProfileID = Value;
    return TRUE;
}

//==============================================================================

save_data_status DecodeProfileStatus( const xarray<u8>& Bytes,
                                      player_profile&   Profile )
{
    xstring Error;
    if( !save_data_codec::DecodeProfile( Bytes, Profile, Error ) )
    {
        LOG_WARNING( "save_data_mgr", "Rejected profile: %s", (const char*)Error );
        return save_data_status::Corrupt;
    }
    return save_data_status::Success;
}

} // namespace

//==============================================================================

struct save_data_mgr::impl
{
    struct request
    {
        request( void ) :
            Owner   ( NULL ),
            PlayerID( -1 )
        {
        }

        void*                Owner;
        completion_callback  Callback;
        save_data_result     Result;
        s32                  PlayerID;
        profile_info         ProfileInfo;
        player_profile       Profile;
        global_settings      Settings;
        xarray<profile_info> Profiles;
    };

    impl( void ) :
        Initialized      ( FALSE ),
        Job              ( HNULL ),
        CompletionPending( FALSE )
    {
        for( s32 i = 0; i < SM_PROFILE_COUNT; i++ )
        {
            SelectedProfiles[i].bDamaged    = FALSE;
            SelectedProfiles[i].ProfileID   = -1;
            SelectedProfiles[i].Hash        = 0;
            SelectedProfiles[i].CreationDate = 0;
            SelectedProfiles[i].ModifiedDate = 0;
        }
    }

    static void ProcessRequest( void* pData );
    static void StartNext     ( impl& Impl );
    static void Add           ( impl& Impl, request& Request );

    save_data_backend   Backend;
    xbool               Initialized;
    xhandle             Job;
    xbool               CompletionPending;
    std::deque<request> Requests;
    xarray<profile_info> Profiles;
    profile_info        SelectedProfiles[SM_PROFILE_COUNT];
    save_data_result    LastResult;
};

save_data_mgr g_SaveDataMgr;

//==============================================================================

static 
save_data_status ScanProfiles( save_data_backend& Backend,
                               xarray<profile_info>& Profiles )
{
    xarray<save_data_file_info> Files;
    const save_data_status ListStatus = Backend.List( Files );
    if( ListStatus != save_data_status::Success )
    {
        return ListStatus;
    }

    std::vector<profile_info> SortedProfiles;
    for( s32 i = 0; i < Files.GetCount(); i++ )
    {
        s32 ProfileID = -1;
        if( !ParseProfileID( Files[i].Name, ProfileID ) )
        {
            continue;
        }

        profile_info Info = {};
        Info.ProfileID    = ProfileID;
        Info.CreationDate = Files[i].CreationDate;
        Info.ModifiedDate = Files[i].ModifiedDate;

        xarray<u8> Bytes;
        player_profile Profile;
        const save_data_status ReadStatus = Backend.Read( Files[i].Name, Bytes );
        const save_data_status DecodeStatus = ReadStatus == save_data_status::Success
            ? DecodeProfileStatus( Bytes, Profile )
            : ReadStatus;

        if( DecodeStatus == save_data_status::Success )
        {
            Info.bDamaged = FALSE;
            Info.Name     = xwstring( Profile.GetProfileName() );
            Info.Hash     = Profile.GetHash();
        }
        else
        {
            Info.bDamaged = TRUE;
            Info.Name     = xwstring( xfs( "Profile %d", ProfileID + 1 ) );
            Info.Hash     = 0;
        }
        SortedProfiles.push_back( Info );
    }

    std::sort( SortedProfiles.begin(), SortedProfiles.end(),
        []( const profile_info& A, const profile_info& B )
        {
            return A.ProfileID < B.ProfileID;
        } );

    Profiles.Clear();
    Profiles.SetCapacity( SAVE_DATA_PROFILE_CAPACITY );
    for( const profile_info& Info : SortedProfiles )
    {
        Profiles.Append() = Info;
    }
    return save_data_status::Success;
}

//==============================================================================

static 
s32 FindFreeProfileID( const xarray<profile_info>& Profiles )
{
    xbool Used[SAVE_DATA_PROFILE_CAPACITY] = {};
    for( s32 i = 0; i < Profiles.GetCount(); i++ )
    {
        const s32 ID = Profiles[i].ProfileID;
        if( (ID >= 0) && (ID < SAVE_DATA_PROFILE_CAPACITY) )
        {
            Used[ID] = TRUE;
        }
    }
    for( s32 ID = 0; ID < SAVE_DATA_PROFILE_CAPACITY; ID++ )
    {
        if( !Used[ID] )
        {
            return ID;
        }
    }
    return -1;
}

//==============================================================================

void save_data_mgr::impl::ProcessRequest( void* pData )
{
    save_data_mgr::impl::request& Request = *(save_data_mgr::impl::request*)pData;
    save_data_backend Backend;
    Request.Result.Status = save_data_status::IoError;

    if( Backend.Init() != save_data_status::Success )
    {
        return;
    }

    switch( Request.Result.Operation )
    {
    case save_data_operation::RefreshProfiles:
        Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
        break;

    case save_data_operation::CreateProfile:
        Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
        if( Request.Result.Status == save_data_status::Success )
        {
            const s32 ProfileID = FindFreeProfileID( Request.Profiles );
            if( ProfileID < 0 )
            {
                Request.Result.Status = save_data_status::NoSpace;
                break;
            }

            xarray<u8> Bytes;
            xstring Error;
            if( !save_data_codec::EncodeProfile( Request.Profile, Bytes, Error ) )
            {
                Request.Result.Status = save_data_status::IoError;
                break;
            }
            Request.Result.ProfileID = ProfileID;
            Request.ProfileInfo.ProfileID = ProfileID;
            Request.Result.Status = Backend.WriteAtomic( GetProfileFileName( ProfileID ), Bytes );
            if( Request.Result.Status == save_data_status::Success )
            {
                Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
            }
        }
        break;

    case save_data_operation::LoadProfile:
        {
            xarray<u8> Bytes;
            Request.Result.Status = Backend.Read(
                GetProfileFileName( Request.ProfileInfo.ProfileID ), Bytes );
            if( Request.Result.Status == save_data_status::Success )
            {
                Request.Result.Status = DecodeProfileStatus(
                    Bytes, Request.Profile );
            }
        }
        break;

    case save_data_operation::SaveProfile:
        {
            xarray<u8> Bytes;
            xstring Error;
            if( !save_data_codec::EncodeProfile( Request.Profile, Bytes, Error ) )
            {
                Request.Result.Status = save_data_status::IoError;
                break;
            }
            Request.Result.Status = Backend.WriteAtomic(
                GetProfileFileName( Request.ProfileInfo.ProfileID ), Bytes );
            if( Request.Result.Status == save_data_status::Success )
            {
                Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
            }
        }
        break;

    case save_data_operation::DeleteProfile:
        Request.Result.Status = Backend.Delete(
            GetProfileFileName( Request.ProfileInfo.ProfileID ) );
        if( Request.Result.Status == save_data_status::Success )
        {
            Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
        }
        break;

    case save_data_operation::SaveSettings:
        {
            xarray<u8> Bytes;
            xstring Error;
            if( !save_data_codec::EncodeSettings( Request.Settings, Bytes, Error ) )
            {
                Request.Result.Status = save_data_status::IoError;
                break;
            }
            Request.Result.Status = Backend.WriteAtomic( SETTINGS_FILE_NAME, Bytes );
        }
        break;

    default:
        Request.Result.Status = save_data_status::IoError;
        break;
    }
}

//==============================================================================

save_data_mgr::save_data_mgr( void ) : m_pImpl( NULL )
{
}

//==============================================================================

save_data_mgr::~save_data_mgr( void )
{
    Kill();
    delete m_pImpl;
    m_pImpl = NULL;
}

//==============================================================================

void save_data_mgr::Init( void )
{
    if( !m_pImpl )
    {
        m_pImpl = new impl;
    }
    if( m_pImpl->Initialized )
    {
        return;
    }
    m_pImpl->Initialized =
        (m_pImpl->Backend.Init() == save_data_status::Success);
    m_pImpl->Job = xhandle( HNULL );
}

//==============================================================================

void save_data_mgr::Kill( void )
{
    if( !m_pImpl )
    {
        return;
    }
    if( m_pImpl->Job.IsNonNull() )
    {
        x_WorkerJobWait( m_pImpl->Job );
        x_WorkerJobRelease( m_pImpl->Job );
        m_pImpl->Job = xhandle( HNULL );
    }
    m_pImpl->Requests.clear();
    m_pImpl->CompletionPending = FALSE;
    m_pImpl->Backend.Kill();
    m_pImpl->Initialized = FALSE;
}

//==============================================================================

xbool save_data_mgr::LoadStartupSettings( void )
{
    ASSERT( m_pImpl && m_pImpl->Initialized );
    xarray<u8> Bytes;
    const save_data_status ReadStatus = m_pImpl->Backend.Read( SETTINGS_FILE_NAME, Bytes );
    if( ReadStatus != save_data_status::Success )
    {
        return FALSE;
    }

    global_settings Settings;
    xstring Error;
    if( !save_data_codec::DecodeSettings( Bytes, Settings, Error ) )
    {
        LOG_WARNING( "save_data_mgr::LoadStartupSettings",
                     "Rejected settings: %s", (const char*)Error );
        return FALSE;
    }

    g_StateMgr.GetActiveSettings() = Settings;
    return TRUE;
}

//==============================================================================

void save_data_mgr::impl::StartNext( impl& Impl )
{
    if( Impl.Job.IsNonNull() || Impl.Requests.empty() )
    {
        return;
    }

    save_data_mgr::impl::request& Request = Impl.Requests.front();
    if( !x_WorkerJobSubmit( ProcessRequest,
                            &Request,
                            "SaveData",
                            Impl.Job ) )
    {
        Request.Result.Status = save_data_status::Busy;
        Impl.CompletionPending = TRUE;
    }
}

//==============================================================================

void save_data_mgr::Update( f32 DeltaTime )
{
    (void)DeltaTime;
    ASSERT( m_pImpl && m_pImpl->Initialized );
    impl::StartNext( *m_pImpl );
    if( m_pImpl->Job.IsNonNull() )
    {
        if( !x_WorkerJobIsDone( m_pImpl->Job ) )
        {
            return;
        }

        x_WorkerJobWait( m_pImpl->Job );
        x_WorkerJobRelease( m_pImpl->Job );
        m_pImpl->Job = xhandle( HNULL );
        m_pImpl->CompletionPending = TRUE;
    }

    if( !m_pImpl->CompletionPending )
    {
        return;
    }
    m_pImpl->CompletionPending = FALSE;

    impl::request& Request = m_pImpl->Requests.front();
    m_pImpl->LastResult = Request.Result;

    if( Request.Result.Status == save_data_status::Success )
    {
        switch( Request.Result.Operation )
        {
        case save_data_operation::RefreshProfiles:
        case save_data_operation::CreateProfile:
        case save_data_operation::SaveProfile:
        case save_data_operation::DeleteProfile:
            m_pImpl->Profiles = Request.Profiles;
            break;
        default:
            break;
        }

        if( (Request.Result.Operation == save_data_operation::LoadProfile) &&
            (Request.PlayerID >= 0) &&
            (Request.PlayerID < SM_PROFILE_COUNT) )
        {
            g_StateMgr.GetPendingProfile() = Request.Profile;
            m_pImpl->SelectedProfiles[Request.PlayerID] = Request.ProfileInfo;
        }
        else if( (Request.Result.Operation == save_data_operation::CreateProfile) &&
                 (Request.PlayerID >= 0) &&
                 (Request.PlayerID < SM_PROFILE_COUNT) )
        {
            for( s32 i = 0; i < m_pImpl->Profiles.GetCount(); i++ )
            {
                if( m_pImpl->Profiles[i].ProfileID == Request.Result.ProfileID )
                {
                    m_pImpl->SelectedProfiles[Request.PlayerID] = m_pImpl->Profiles[i];
                    break;
                }
            }
        }
        else if( (Request.Result.Operation == save_data_operation::SaveProfile) &&
                 (Request.PlayerID >= 0) &&
                 (Request.PlayerID < SM_PROFILE_COUNT) )
        {
            for( s32 i = 0; i < m_pImpl->Profiles.GetCount(); i++ )
            {
                if( m_pImpl->Profiles[i].ProfileID == Request.ProfileInfo.ProfileID )
                {
                    m_pImpl->SelectedProfiles[Request.PlayerID] = m_pImpl->Profiles[i];
                    break;
                }
            }
        }
        else if( Request.Result.Operation == save_data_operation::DeleteProfile )
        {
            for( s32 i = 0; i < SM_PROFILE_COUNT; i++ )
            {
                if( m_pImpl->SelectedProfiles[i].ProfileID == Request.ProfileInfo.ProfileID )
                {
                    m_pImpl->SelectedProfiles[i].bDamaged     = FALSE;
                    m_pImpl->SelectedProfiles[i].ProfileID    = -1;
                    m_pImpl->SelectedProfiles[i].Hash         = 0;
                    m_pImpl->SelectedProfiles[i].CreationDate = 0;
                    m_pImpl->SelectedProfiles[i].ModifiedDate = 0;
                    m_pImpl->SelectedProfiles[i].Name.Clear();
                }
            }
        }
        else if( Request.Result.Operation == save_data_operation::SaveSettings )
        {
            g_StateMgr.GetPendingSettings().Checksum();
            g_StateMgr.GetActiveSettings().Checksum();
        }
    }

    completion_callback Callback = Request.Callback;
    const save_data_result Result = Request.Result;
    m_pImpl->Requests.pop_front();
    impl::StartNext( *m_pImpl );
    if( Callback )
    {
        Callback( Result );
    }
}

//==============================================================================

void save_data_mgr::impl::Add( impl& Impl, request& Request )
{
    if( (s32)Impl.Requests.size() >= SAVE_DATA_MAX_PENDING_REQUESTS )
    {
        Impl.LastResult.Operation = Request.Result.Operation;
        Impl.LastResult.Status    = save_data_status::Busy;
        Impl.LastResult.ProfileID = Request.Result.ProfileID;
        if( Request.Callback )
        {
            Request.Callback( Impl.LastResult );
        }
        return;
    }

    Impl.Requests.push_back( Request );
    StartNext( Impl );
}

//==============================================================================

void save_data_mgr::QueueRefreshProfiles( void* pOwner,
                                          completion_callback Callback )
{
    impl::request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.Result.Operation = save_data_operation::RefreshProfiles;
    impl::Add( *m_pImpl, Request );
}

//==============================================================================

void save_data_mgr::RefreshProfiles( void )
{
    QueueRefreshProfiles( NULL, completion_callback() );
}

//==============================================================================

void save_data_mgr::QueueCreateProfile( s32 PlayerID,
                                        void* pOwner,
                                        completion_callback Callback )
{
    ASSERT( (PlayerID >= 0) && (PlayerID < SM_PROFILE_COUNT) );
    impl::request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.PlayerID         = PlayerID;
    Request.Profile          = g_StateMgr.GetPendingProfile();
    Request.Result.Operation = save_data_operation::CreateProfile;
    impl::Add( *m_pImpl, Request );
}

//==============================================================================

void save_data_mgr::QueueProfileOperation(
    save_data_operation Operation,
    const profile_info& Info,
    s32 PlayerID,
    void* pOwner,
    completion_callback Callback )
{
    impl::request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.PlayerID         = PlayerID;
    Request.ProfileInfo      = Info;
    Request.Result.Operation = Operation;
    Request.Result.ProfileID = Info.ProfileID;
    if( Operation == save_data_operation::SaveProfile )
    {
        Request.Profile = g_StateMgr.GetPendingProfile();
    }
    impl::Add( *m_pImpl, Request );
}

//==============================================================================

void save_data_mgr::QueueSaveSettings( void* pOwner,
                                       completion_callback Callback )
{
    impl::request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.Settings         = g_StateMgr.GetPendingSettings();
    Request.Result.Operation = save_data_operation::SaveSettings;
    impl::Add( *m_pImpl, Request );
}

//==============================================================================

void save_data_mgr::CancelCallbacks( void* pOwner )
{
    for( impl::request& Request : m_pImpl->Requests )
    {
        if( Request.Owner == pOwner )
        {
            Request.Callback = completion_callback();
            Request.Owner    = NULL;
        }
    }
}

//==============================================================================

void save_data_mgr::ClearCachedProfiles( void )
{
    m_pImpl->Profiles.Clear();
}

//==============================================================================

xbool save_data_mgr::IsBusy( void ) const
{
    return m_pImpl &&
           (!m_pImpl->Requests.empty() || m_pImpl->Job.IsNonNull());
}

//==============================================================================

const save_data_result& save_data_mgr::GetLastResult( void ) const
{
    return m_pImpl->LastResult;
}

//==============================================================================

profile_info& save_data_mgr::GetProfileInfo( s32 PlayerID )
{
    ASSERT( (PlayerID >= 0) && (PlayerID < SM_PROFILE_COUNT) );
    return m_pImpl->SelectedProfiles[PlayerID];
}

//==============================================================================

void save_data_mgr::GetProfileNames( xarray<profile_info*>& Result )
{
    Result.SetCount( 0 );
    for( s32 i = 0; i < m_pImpl->Profiles.GetCount(); i++ )
    {
        Result.Append( &m_pImpl->Profiles[i] );
    }
}
