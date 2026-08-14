//==============================================================================
//
//  SaveDataMgr.hpp
//
//==============================================================================

#ifndef SAVE_DATA_MGR_HPP
#define SAVE_DATA_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SaveDataTypes.hpp"
#include "StateMgr/PlayerProfile.hpp"

#include <functional>

//==============================================================================
//  SAVE DATA MGR
//==============================================================================

class save_data_mgr
{
public:
    save_data_mgr ( void );
    ~save_data_mgr( void );

    void  Init                ( void );
    void  Kill                ( void );
    void  Update              ( f32 DeltaTime );
    xbool LoadStartupSettings ( void );

    void RefreshProfiles( void );

    template<class TYPE>
    void RefreshProfiles( TYPE* pObject, void (TYPE::*pMethod)(void) )
    {
        QueueRefreshProfiles( pObject, Bind( pObject, pMethod ) );
    }

    template<class TYPE>
    void CreateProfile( s32 PlayerID,
                        TYPE* pObject,
                        void (TYPE::*pMethod)(void) )
    {
        QueueCreateProfile( PlayerID, pObject, Bind( pObject, pMethod ) );
    }

    template<class TYPE>
    void LoadProfile( const profile_info& Info,
                      s32 PlayerID,
                      TYPE* pObject,
                      void (TYPE::*pMethod)(void) )
    {
        QueueProfileOperation( save_data_operation::LoadProfile,
                               Info, PlayerID, pObject,
                               Bind( pObject, pMethod ) );
    }

    template<class TYPE>
    void SaveProfile( const profile_info& Info,
                      s32 PlayerID,
                      TYPE* pObject,
                      void (TYPE::*pMethod)(void) )
    {
        QueueProfileOperation( save_data_operation::SaveProfile,
                               Info, PlayerID, pObject,
                               Bind( pObject, pMethod ) );
    }

    template<class TYPE>
    void OverwriteProfile( const profile_info& Info,
                           s32 PlayerID,
                           TYPE* pObject,
                           void (TYPE::*pMethod)(void) )
    {
        SaveProfile( Info, PlayerID, pObject, pMethod );
    }

    template<class TYPE>
    void DeleteProfile( const profile_info& Info,
                        TYPE* pObject,
                        void (TYPE::*pMethod)(void) )
    {
        QueueProfileOperation( save_data_operation::DeleteProfile,
                               Info, -1, pObject,
                               Bind( pObject, pMethod ) );
    }

    template<class TYPE>
    void SaveSettings( TYPE* pObject,
                       void (TYPE::*pMethod)(void) )
    {
        QueueSaveSettings( pObject, Bind( pObject, pMethod ) );
    }

    void                    CancelCallbacks     ( void* pOwner );
    void                    ClearCachedProfiles ( void );
    xbool                   IsBusy              ( void ) const;
    const save_data_result& GetLastResult       ( void ) const;
    profile_info&           GetProfileInfo      ( s32 PlayerID );
    void                    GetProfileNames     ( xarray<profile_info*>& Result );

private:
    struct impl;

    typedef std::function<void(const save_data_result&)> completion_callback;

    template<class TYPE>
    static completion_callback Bind( TYPE* pObject, void (TYPE::*pMethod)(void) )
    {
        return [pObject, pMethod]( const save_data_result& )
        {
            (pObject->*pMethod)();
        };
    }

    void QueueRefreshProfiles  ( void* pOwner,
                                 completion_callback Callback );
    void QueueCreateProfile    ( s32 PlayerID,
                                 void* pOwner,
                                 completion_callback Callback );
    void QueueProfileOperation ( save_data_operation Operation,
                                 const profile_info& Info,
                                 s32 PlayerID,
                                 void* pOwner,
                                 completion_callback Callback );
    void QueueSaveSettings     ( void* pOwner,
                                 completion_callback Callback );

    impl* m_pImpl;
};

//==============================================================================
extern save_data_mgr g_SaveDataMgr;
//==============================================================================

//==============================================================================
#endif // SAVE_DATA_MGR_HPP
//==============================================================================
