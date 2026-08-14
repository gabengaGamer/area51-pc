#ifndef LEVEL_LOADER_HPP
#define LEVEL_LOADER_HPP

#include "x_files.hpp"

class map_entry;
class text_in;

class level_loader
{
public:
                level_loader                ( void );
               ~level_loader                ( void );
    void        BeginLevelLoad              ( xbool         bFullLoad );
    void        UpdateLevelLoad             ( f32           TimeBudgetSeconds );
    xbool       IsLevelLoadComplete         ( void ) const;
    void        LoadLevelFinish             ( void );
    void        UnloadLevel                 ( xbool         bFullUnload );
    void        LoadDFS                     ( const char*   pDFS );
    void        MountDefaultFilesystems     ( void );
    void        UnmountDefaultFilesystems   ( void );

private:
    enum level_load_stage
    {
        LEVEL_LOAD_IDLE,
        LEVEL_LOAD_FIND_MAP,
        LEVEL_LOAD_MOUNT_FILESYSTEM,
        LEVEL_LOAD_INITIALIZE_RENDER,
        LEVEL_LOAD_OPEN_SCRIPT,
        LEVEL_LOAD_EXECUTE_SCRIPT,
        LEVEL_LOAD_SCRIPT_DFS,
        LEVEL_LOAD_INITIALIZE_DATA,
        LEVEL_LOAD_LOAD_TWEAKS,
        LEVEL_LOAD_LOAD_PAIN,
        LEVEL_LOAD_CREATE_OBJECTS,
        LEVEL_LOAD_NAV_MAP,
        LEVEL_LOAD_GLOBALS,
        LEVEL_LOAD_PRELOAD_RIGID_COLORS,
        LEVEL_LOAD_INFO,
        LEVEL_LOAD_BEGIN_BINARY_LEVEL,
        LEVEL_LOAD_BINARY_LEVEL,
        LEVEL_LOAD_RIGID_COLORS,
        LEVEL_LOAD_TEMPLATES,
        LEVEL_LOAD_ZONES,
        LEVEL_LOAD_BEGIN_PLAY_SURFACES,
        LEVEL_LOAD_PLAY_SURFACES,
        LEVEL_LOAD_DECALS,
        LEVEL_LOAD_PLAYER_ZONES,
        LEVEL_LOAD_FINALIZE_CORE,
        LEVEL_LOAD_FINALIZE_RUNTIME,
        LEVEL_LOAD_FINALIZE_OCCLUDERS,
        LEVEL_LOAD_UNMOUNT_FILESYSTEM,
        LEVEL_LOAD_COMPLETE,
        LEVEL_LOAD_FAILED
    };

    void        LoadInfo                    ( const char*   pPath );
    void        PrepareSlideshow            ( const char*   pSlideShowScriptFile );
    void        StartSlideshow              ( void );
    void        LoadDFSResource             ( s32           FileSystem,
                                              s32           FileIndex );
    void        BeginScriptDFSLoad          ( const char*   pDFS );
    void        UpdateScriptDFSLoad         ( void );
    void        CloseLoadScript             ( void );
    void        FailLevelLoad               ( void );

    level_load_stage m_LevelLoadStage;
    const map_entry* m_pMapEntry;
    text_in*         m_pLoadScript;
    xstring          m_LevelFileSystem;
    xstring          m_ScriptDFS;
    s32              m_LoadScriptCommand;
    s32              m_LoadScriptCommandCount;
    s32              m_ScriptDFSFileSystem;
    s32              m_ScriptDFSFile;
    s32              m_ScriptDFSFileCount;
    s32              m_NextPlayerSlot;
    s32              m_VoiceID;
    xbool            m_bFullLoad;
    xbool            m_SlideshowPrepared;
    xbool            m_LevelFileSystemMounted;
};

extern level_loader g_LevelLoader;

#endif // LEVEL_LOADER_HPP
