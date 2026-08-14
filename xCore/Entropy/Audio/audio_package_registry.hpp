//==============================================================================
//
//  audio_package_registry.hpp
//
//==============================================================================

#ifndef AUDIO_PACKAGE_REGISTRY_HPP
#define AUDIO_PACKAGE_REGISTRY_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "Audio/audio_package.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct audio_runtime;

//==============================================================================
//  AUDIO PACKAGE REGISTRY CLASS
//==============================================================================

class audio_package_registry
{
public:

                            audio_package_registry  ( void );
                           ~audio_package_registry  ( void );

            void            Init                    ( audio_runtime&    Runtime );
            void            Kill                    ( void );

            xbool           LoadPackage             ( const char*       pFilename,
                                                      const char*       pLocalizedName );
            xbool           UnloadPackage           ( const char*       pFilename );
            xbool           IsPackageLoaded         ( const char*       pFilename );
            xbool           LoadPackageStrings      ( const char*       pFilename,
                                                      const char*       pLocalizedName,
                                                      xarray<xstring>&  Strings );
            void            UnloadAllPackages       ( void );
            void            GetLoadedPackages       ( xarray<xstring>&  Packages );
            void            GetLoadedPackageLookupNames
                                                    ( xarray<xstring>&  Packages );
            void            DisplayPackages         ( void );
            s32             GetPackageARAM          ( const char*       pPackage );
            char*           GetMusicType            ( const char*       pFilename );
            s32             GetMusicIntensity       ( const char*       pFilename,
                                                      music_intensity* &Intensity );

            audio_package*  FindPackageByName       ( const char*       pFilename );
            u16*            FindDescriptorByName    ( const char*       pName,
                                                      audio_package**   pPackageResult,
                                                      char* &           DescriptorName );
            xbool           IsValidDescriptor       ( const char*       pIdentifier );
            void            ReMergeIdentifierTables ( void );

            void            SetMasterVolume         ( f32               Volume );
            void            SetMusicVolume          ( f32               Volume );
            void            SetSFXVolume            ( f32               Volume );
            void            SetVoiceVolume          ( f32               Volume );
            void            ComputePackageVolumes   ( void );

private:

inline      audio_runtime&  Runtime                 ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }

            void            ResetPackageList        ( void );
            void            MergeIdentifierTables   ( void );

private:

xarray<descriptor_identifier*>  m_pIdentifiers;
audio_package::package_link     m_Link;
audio_runtime*                  m_pRuntime;
};

//==============================================================================
#endif // AUDIO_PACKAGE_REGISTRY_HPP
//==============================================================================
