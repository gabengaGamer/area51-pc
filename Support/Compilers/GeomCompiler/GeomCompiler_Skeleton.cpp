#include "GeomCompiler.hpp"
#include "RawSettings.hpp"

extern xbool g_Verbose;

struct bone_to_hit_mapping
{
    const char*                 pBoneNameFragment;
    geom::bone::hit_location    HitLocation;
};

static 
bone_to_hit_mapping g_BoneHitLocationMapping[] = 
{
    { "Leg",        geom::bone::HIT_LOCATION_LEGS           },

    { "Spine",      geom::bone::HIT_LOCATION_TORSO          },      

    { "Neck",       geom::bone::HIT_LOCATION_HEAD           },
    { "Head",       geom::bone::HIT_LOCATION_HEAD           },

    { "Arm_L",      geom::bone::HIT_LOCATION_SHOULDER_LEFT  },
    { "L_Forearm",  geom::bone::HIT_LOCATION_SHOULDER_LEFT  },
    { "L_Shoulder", geom::bone::HIT_LOCATION_SHOULDER_LEFT  },

    { "Arm_R",      geom::bone::HIT_LOCATION_SHOULDER_RIGHT },
    { "R_Forearm",  geom::bone::HIT_LOCATION_SHOULDER_RIGHT },
    { "R_Shoulder", geom::bone::HIT_LOCATION_SHOULDER_RIGHT },   

    // Dust Mites
    { "Body",       geom::bone::HIT_LOCATION_TORSO          },      
    { "Jaw",        geom::bone::HIT_LOCATION_HEAD           },      

};

//=============================================================================

struct object_to_object_mapping
{
    const char* pObjectA;
    const char* pObjectB;
};

// NOTE: This table is used to map Bone->RigidBody mapping and RigidBody->Bone mapping
static 
object_to_object_mapping g_BoneToRigidBodyMapping[] = 
{
    // Bone             // RigidBody
    
    // Extra theta bones
    { "Leg_L_Foot_Squash01",    "L_Foot"                },
    { "Leg_L_Foot_Squash02",    "L_Foot"                },
    { "Leg_L_Foot",             "L_Foot"                },
    { "Leg_L_Calf01",           "L_Calf01"              },
    { "Leg_L_Calf02",           "L_Calf02"              },
    { "Leg_L_Thigh",            "L_Thigh"               },

    { "ChstArm_L_UpperArm",     "L_ChstArm_UpperArm"    },
    { "ChstArm_L_ForeArm",      "L_ChstArm_LowerArm"    },
    { "ChstArm_L_Hand",         "L_ChstArm_Hand"        },
    { "ChstArm_L_Finger",       "L_ChstArm_Hand"        },
    { "ChstArm_L_Thumb",        "L_ChstArm_Hand"        },
    
    { "Leg_R_Foot_Squash01",    "R_Foot"                },
    { "Leg_R_Foot_Squash02",    "R_Foot"                },
    { "Leg_R_Foot",             "R_Foot"                },
    { "Leg_R_Calf01",           "R_Calf01"              },
    { "Leg_R_Calf02",           "R_Calf02"              },
    { "Leg_R_Thigh",            "R_Thigh"               },

    { "ChstArm_R_UpperArm",     "R_ChstArm_UpperArm"    },
    { "ChstArm_R_ForeArm",      "R_ChstArm_LowerArm"    },
    { "ChstArm_R_Hand",         "R_ChstArm_Hand"        },
    { "ChstArm_R_Finger",       "R_ChstArm_Hand"        },
    { "ChstArm_R_Thumb",        "R_ChstArm_Hand"        },

    { "Neck",                   "Neck"                  },
    { "spore",                  "Torso"                 },
    
    // Normal characters
    { "Root",                   "Pelvis"        },
    { "Spine01",                "Pelvis"        },
    { "Spine02",                "Torso"         },
    { "Head",                   "Head"          },
    { "Neck",                   "Torso"         },
    { "Face",                   "Head"          },
    { "Jaw",                    "Head"          },

    { "Arm_R_UpperArm",         "R_UpperArm"    },
    { "Arm_R_ForeArm",          "R_LowerArm"    },
    { "R_Forearm",              "R_LowerArm"    },
    { "Arm_R_Hand",             "R_Hand"        },
    { "Hand_R",                 "R_Hand"        },
    { "Attach_R",               "R_Hand"        },

    { "Arm_L_UpperArm",         "L_UpperArm"    },
    { "Arm_L_ForeArm",          "L_LowerArm"    },
    { "L_Forearm",              "L_LowerArm"    },
    { "Arm_L_Hand",             "L_Hand"        },
    { "Hand_L",                 "L_Hand"        },
    { "Attach_L",               "L_Hand"        },

    { "Leg_R_Thigh",            "R_Thigh"       },
    { "Leg_R_Calf",             "R_Calf"        },
    { "Leg_R_Foot",             "R_Foot"        },
    { "Leg_R_Toe",              "R_Foot"        },

    { "Leg_L_Thigh",            "L_Thigh"       },
    { "Leg_L_Calf",             "L_Calf"        },
    { "Leg_L_Foot",             "L_Foot"        },
    { "Leg_L_Toe",              "L_Foot"        },

    { "L_Shoulder",             "Torso"         },
    { "R_Shoulder",             "Torso"         },
    { "Body",                   "Torso"         },
};

//=============================================================================

void geom_compiler::BuildBone( geom::bone& Bone, const rawmesh2::bone& RawBone )
{
    // Grab bind info
    Bone.BindRotation = RawBone.Rotation;
    Bone.BindPosition = RawBone.Position;

    // Grab bbox
    Bone.BBox = RawBone.BBox;

    // Clear hit location and rigid body index
    Bone.HitLocation = geom::bone::HIT_LOCATION_UNKNOWN;
    Bone.iRigidBody  = -1;
}

//=============================================================================

void geom_compiler::BuildBones(       geom&     Geom, 
                                const rawmesh2& GeomRawMesh,
                                const rawmesh2& PhysicsRawMesh )
{
    s32 i, j, nMaps;

    // Get bone count
    s32 nBones = GeomRawMesh.m_nBones ;
    if(! nBones )
        return;

    // Create bones
    Geom.m_nBones = nBones ;
    Geom.m_pBone  = new geom::bone[nBones] ;
    if( Geom.m_pBone == NULL )
        x_throw( "Out of memory" );

    // Setup bones
    for( i = 0 ; i < nBones ; i++ )
        BuildBone( Geom.m_pBone[i], GeomRawMesh.m_pBone[i] );

    // Setup hit locations
    for( i = 0; i < nBones; i++ )
    {
        // Loop through all maps
        nMaps = sizeof(g_BoneHitLocationMapping) / sizeof(bone_to_hit_mapping);
        for( j = 0; j < nMaps; j++ )
        {
            // Found bone in mapping table?
            if( x_stristr( GeomRawMesh.m_pBone[i].Name, g_BoneHitLocationMapping[j].pBoneNameFragment ) )
            {
                // Map bone if not already assigned
                geom::bone& Bone = Geom.m_pBone[i];
                if( Bone.HitLocation == geom::bone::HIT_LOCATION_UNKNOWN )
                    Bone.HitLocation = g_BoneHitLocationMapping[j].HitLocation;
            }            
        }
    }
    
    // Setup bone -> rigid body mapping
    if( PhysicsRawMesh.m_nRigidBodies )
    {
        // Loop through bones
        for( i = 0; i < nBones; i++ )
        {
            // Loop through map table
            nMaps = sizeof(g_BoneToRigidBodyMapping) / sizeof(object_to_object_mapping);
            for( j = 0; j < nMaps; j++ )
            {
                // Found bone in mapping table?
                if( x_stristr( GeomRawMesh.m_pBone[i].Name, g_BoneToRigidBodyMapping[j].pObjectA ) )
                {
                    // Try map bone if not already assigned
                    geom::bone& Bone = Geom.m_pBone[i];
                    if( Bone.iRigidBody == -1 )
                        Bone.iRigidBody = PhysicsRawMesh.GetRigidBodyIDFromName( g_BoneToRigidBodyMapping[j].pObjectB, TRUE );
                }                    
            }
        }
            
        // Validate bone -> rigid body mapping
        for( i = 0; i < nBones; i++ )
        {
            // Lookup bone
            geom::bone& Bone = Geom.m_pBone[i];
        
            // Not mapped yet?
            if( Bone.iRigidBody == -1 )
            {
                // Try map bone to matching rigid body name
                const char* pBoneName = GeomRawMesh.m_pBone[i].Name;
                while( ( Bone.iRigidBody == -1 ) && ( pBoneName[0] ) )
                {
                    // Try lookup rigid body
                    Bone.iRigidBody = PhysicsRawMesh.GetRigidBodyIDFromName( pBoneName, TRUE );
                    
                    // Goto next character in bone name
                    pBoneName++;
                }
                
                // Still not found?
                if( Bone.iRigidBody == -1 )
                {
                    // Map to root rigid body
                    Bone.iRigidBody = 0;

                    // Show warning
                    ReportWarning( xfs("Bone [%s] -> RigidBody mapping not found, defaulting to root rigid body.", GeomRawMesh.m_pBone[i].Name ) );
                }
            }
        }
    }
    
    // Show bone->rigid body mappings?
    if( ( g_Verbose ) && ( PhysicsRawMesh.m_nRigidBodies ) )
    {
        x_printf( "\nBone->RigidBody mappings:\n" );
        for( i = 0; i < nBones; i++ )
        {
            // Lookup bone
            geom::bone& Bone = Geom.m_pBone[i];
            if( Bone.iRigidBody != -1 )
            {
                x_printf( "\"%s\" mapped to \"%s\"\n", 
                          GeomRawMesh.m_pBone[i].Name,
                          PhysicsRawMesh.m_pRigidBodies[Bone.iRigidBody].Name );
            }                          
            else                                
            {
                x_printf("\"%s\" mapped to NULL!\n", GeomRawMesh.m_pBone[i].Name );
            }
        }    
        x_printf( "\n\n" );
    }
}
    
//=============================================================================

void geom_compiler::BuildRigidBody( geom::rigid_body& RigidBody, const rawmesh2::rigid_body& RawRigidBody )
{
    // Grab properties
    RigidBody.BodyBindRotation  = RawRigidBody.BodyRotation;
    RigidBody.BodyBindPosition  = RawRigidBody.BodyPosition;
    RigidBody.PivotBindRotation = RawRigidBody.PivotRotation;
    RigidBody.PivotBindPosition = RawRigidBody.PivotPosition;
    RigidBody.NameOffset        = m_Dictionary.Add( RawRigidBody.Name );
    RigidBody.Mass              = RawRigidBody.Mass;  
    RigidBody.Radius            = RawRigidBody.Radius;
    RigidBody.Width             = RawRigidBody.Width;
    RigidBody.Height            = RawRigidBody.Height;
    RigidBody.Length            = RawRigidBody.Length;
    RigidBody.iParentBody       = RawRigidBody.iParent;    
    RigidBody.iBone             = -1;

    // Bake bind scale into properties so we don't have to export it
    RigidBody.Radius       *= RawRigidBody.BodyScale.GetX(); 
    RigidBody.Width        *= RawRigidBody.BodyScale.GetX();  
    RigidBody.Height       *= RawRigidBody.BodyScale.GetY(); 
    RigidBody.Length       *= RawRigidBody.BodyScale.GetZ(); 
    
    // Setup default flags
    RigidBody.Flags = geom::rigid_body::FLAG_WORLD_COLLISION;
    
    // Turn off collision for rope
    if( x_stristr( RawRigidBody.Name, "RB_Rope" ) == RawRigidBody.Name )
        RigidBody.Flags &= ~geom::rigid_body::FLAG_WORLD_COLLISION;
        
    // Setup type    
    if( x_stricmp( RawRigidBody.Type, "Sphere" ) == 0 )
    {
        RigidBody.Type = geom::rigid_body::TYPE_SPHERE;
    }                
    else if( x_stricmp( RawRigidBody.Type, "Cylinder" ) == 0 )
    {
        RigidBody.Type = geom::rigid_body::TYPE_CYLINDER;
    }        
    else if( x_stricmp( RawRigidBody.Type, "Box" ) == 0 )
    {    
        RigidBody.Type = geom::rigid_body::TYPE_BOX;
    }        
    else        
    {    
        RigidBody.Type = geom::rigid_body::TYPE_SPHERE;
    }        
    
    // Setup default collision mask to all on
    RigidBody.CollisionMask = 0xFFFFFFFF;
    
    // Setup DOFs
    for( s32 i = 0; i < 6; i++ )
    {
        geom::rigid_body::dof&           DOF    = RigidBody.DOF[i];
        const rawmesh2::rigid_body::dof& RawDOF = RawRigidBody.DOF[i];
        
        DOF.Flags = 0;
        
        if( RawDOF.bActive )
            DOF.Flags |= geom::rigid_body::dof::FLAG_ACTIVE;
        
        if( RawDOF.bLimited )
            DOF.Flags |= geom::rigid_body::dof::FLAG_LIMITED;
        
        DOF.Min = RawDOF.Min;
        DOF.Max = RawDOF.Max;
    }
    
}

//=============================================================================

static 
object_to_object_mapping g_RigidBodyDisableCollisionMapping[] = 
{
    { "Head",           "Pelvis"        },
    { "Head",           "L_Thigh"       },
    { "Head",           "R_Thigh"       },
    { "Head",           "L_Foot"        },
    { "Head",           "R_Foot"        },
    { "Head",           "L_Calf"        },
    { "Head",           "R_Calf"        },
                                   
    { "Torso",          "L_Thigh"       },
    { "Torso",          "R_Thigh"       },
    { "Torso",          "L_Calf"        },
    { "Torso",          "R_Calf"        },
    { "Torso",          "L_Foot"        },
    { "Torso",          "R_Foot"        },
                                   
    { "Pelvis",         "L_Calf"        },
    { "Pelvis",         "R_Calf"        },
    { "Pelvis",         "L_Foot"        },
    { "Pelvis",         "R_Foot"        },
                                   
    { "L_Thigh",        "L_Foot"        },
    { "R_Thigh",        "R_Foot"        },
};

//=============================================================================

void geom_compiler::BuildRigidBodies(       geom&     Geom, 
                                      const rawmesh2& GeomRawMesh,
                                      const rawmesh2& PhysicsRawMesh )
{
    s32 i, j, nMaps;
    
    // Lookup # to create
    s32 nRigidBodies = PhysicsRawMesh.m_nRigidBodies;
    if( !nRigidBodies )
        return;
    
    // Create rigid bodies
    Geom.m_nRigidBodies = nRigidBodies;
    Geom.m_pRigidBodies = new geom::rigid_body[nRigidBodies];
    if( Geom.m_pRigidBodies == NULL )
        x_throw( "Out of memory" );

    // Setup rigid bodies
    for( i = 0 ; i < nRigidBodies; i++ )
        BuildRigidBody( Geom.m_pRigidBodies[i], PhysicsRawMesh.m_pRigidBodies[i] );

    // Setup rigid body -> bone mapping
    for( i = 0 ; i < nRigidBodies; i++ )
    {
        // Loop through maps
        nMaps = sizeof(g_BoneToRigidBodyMapping) / sizeof(object_to_object_mapping);
        for( j = 0; j < nMaps; j++ )
        {
            // Found rigid body in mapping table?
            if( x_stristr( PhysicsRawMesh.m_pRigidBodies[i].Name, g_BoneToRigidBodyMapping[j].pObjectB ) )
            {
                // Lookup bone index if it hasn't been mapped yet
                geom::rigid_body& Body = Geom.m_pRigidBodies[i];
                if( Body.iBone == -1 )
                    Body.iBone = GeomRawMesh.GetBoneIDFromName( g_BoneToRigidBodyMapping[j].pObjectA, TRUE );
            }
        }
    }
        
    // Check all RigidBody -> Bone mappings
    for( i = 0; i < nRigidBodies; i++ )
    {
        // Lookup bone
        geom::rigid_body& Body = Geom.m_pRigidBodies[i];
        if( Body.iBone == -1 )
        {
            // Try map bone to matching rigid body name
            const char* pBodyName = PhysicsRawMesh.m_pRigidBodies[i].Name;
            while( ( Body.iBone == -1 ) && ( pBodyName[0] ) )
            {
                // Try lookup bone
                Body.iBone = GeomRawMesh.GetBoneIDFromName( pBodyName, TRUE );

                // Goto next character in rigid body name
                pBodyName++;
            }

            // Still not mapped?
            if( Body.iBone == -1 )            
            {
                // Map to root bone
                Body.iBone = 0;

                // Show warning
                ReportWarning( xfs("RigidBody [%s] -> Bone mapping not found, defaulting to root bone.", PhysicsRawMesh.m_pRigidBodies[i].Name ) );
            }
        }
    }
            
    // Turn off collision between parent and child rigid bodies
    for( i = 0; i < nRigidBodies; i++ )
    {
        // Does body have parent?
        s32 iParentBody = Geom.m_pRigidBodies[i].iParentBody;
        if( iParentBody != -1 )
        {
            // Exclude each body from each others collision mask
            Geom.m_pRigidBodies[i].CollisionMask           &= ~( 1 << iParentBody );
            Geom.m_pRigidBodies[iParentBody].CollisionMask &= ~( 1 << i );
        }
    }
    
    // Turn off collision between specified rigid bodies
    nMaps = sizeof(g_RigidBodyDisableCollisionMapping) / sizeof(object_to_object_mapping);
    for( i = 0; i < nMaps; i++ )
    {
        // Lookup rigid body indices
        s32 iBodyA = PhysicsRawMesh.GetRigidBodyIDFromName( g_RigidBodyDisableCollisionMapping[i].pObjectA, TRUE );
        s32 iBodyB = PhysicsRawMesh.GetRigidBodyIDFromName( g_RigidBodyDisableCollisionMapping[i].pObjectB, TRUE );
        
        // If both found, disable collision between them
        if( ( iBodyA != -1 ) && ( iBodyB != -1 ) )
        {
            Geom.m_pRigidBodies[ iBodyA ].CollisionMask &= ~( 1 << iBodyB );
            Geom.m_pRigidBodies[ iBodyB ].CollisionMask &= ~( 1 << iBodyA );
        }
    }
}

//=============================================================================

void geom_compiler::BuildSettings( geom& Geom, const char* pSettingsFile, const rawmesh2& GeomRawMesh )
{
    // Clear bone masks
    Geom.m_nBoneMasks = 0;
    Geom.m_pBoneMasks = NULL;

    // Clear properties
    Geom.m_nPropertySections = 0;
    Geom.m_pPropertySections = NULL;
    Geom.m_nProperties       = 0;
    Geom.m_pProperties       = NULL;
    
    // No file specified?
    if( !pSettingsFile[0] )
        return;

    // Show info
    if( g_Verbose )
        x_printf( "Parsing settings file [%s]\n", pSettingsFile );
    
    // Try load raw settings
    raw_settings RawSettings;
    if( !RawSettings.Load( pSettingsFile ) )
        return;

    // Compile bone masks?
    if( RawSettings.m_BoneMasks.GetCount() )
    {
        // Show info
        x_printf( "%30s - %10d Bone masks\n", "Compiling...", RawSettings.m_BoneMasks.GetCount() );
        
        // Allocate
        Geom.m_nBoneMasks = RawSettings.m_BoneMasks.GetCount(); 
        Geom.m_pBoneMasks = new geom::bone_masks[ Geom.m_nBoneMasks ];
        if( Geom.m_pBoneMasks == NULL )
            x_throw( "Out of memory" );

        // Compile bone masks
        for( s32 iBoneMasks = 0; iBoneMasks < RawSettings.m_BoneMasks.GetCount(); iBoneMasks++ )
        {
            // Lookup source + dest
            const raw_settings::bone_masks& SrcBoneMasks = RawSettings.m_BoneMasks[ iBoneMasks ];
            geom::bone_masks&     DstBoneMasks = Geom.m_pBoneMasks[ iBoneMasks ];

            // Setup bone masks
            DstBoneMasks.NameOffset = m_Dictionary.Add( SrcBoneMasks.Name );
            DstBoneMasks.nBones     = 0;
            for( s32 iWeight = 0; iWeight < MAX_ANIM_BONES; iWeight++ )
                DstBoneMasks.Weights[iWeight] = 0.0f;

            // Setup mask values for specified bones            
            for( s32 iMask = 0; iMask < SrcBoneMasks.Masks.GetCount(); iMask++ )
            {
                // Lookup source
                const raw_settings::bone_masks::mask& SrcMask = SrcBoneMasks.Masks[iMask];
                
                // Setup
                s32 iBone = GeomRawMesh.GetBoneIDFromName( (const char*)SrcMask.BoneName, TRUE );
                if( iBone != -1 )
                {
                    DstBoneMasks.Weights[iBone] = SrcMask.Weight; 
                    DstBoneMasks.nBones = x_max( DstBoneMasks.nBones, iBone+1 );
                }
                else
                {
                    x_printf( xfs( "Warning: Bone [%s] not found in geometry - mask wil be ignored.\nReferenced in settings file [%s]\n\n", (const char*)SrcMask.BoneName, pSettingsFile ) );
                }                    
            }
        }
        
        // Show compiled info?
        if( g_Verbose )
        {
            // Loop over all bone masks
            x_printf("\nBone Masks:%d\n", Geom.m_nBoneMasks );
            for( s32 i = 0; i < Geom.m_nBoneMasks; i++ )
            {
                // Show group info
                const geom::bone_masks& BoneMasks = Geom.m_pBoneMasks[ i ];
                x_printf("\n    GroupName:%s nBones:%d\n\n", m_Dictionary.GetString( BoneMasks.NameOffset ), BoneMasks.nBones );

                // Show bones and weights
                for( s32 j = 0; j < GeomRawMesh.m_nBones; j++ )
                    x_printf("        Bone:%s Weight:%f\n", GeomRawMesh.m_pBone[j].Name, BoneMasks.Weights[j] );

                x_printf("\n");
            }
            x_printf("\n");
        }            
    }

    // Compile properties?
    if( RawSettings.m_Properties.GetCount() )
    {
        // Show info
        x_printf( "%30s - %10d Properties\n", "Compiling...", RawSettings.m_Properties.GetCount() );
        
        // Allocate property sections
        Geom.m_nPropertySections = RawSettings.m_PropertySections.GetCount();
        Geom.m_pPropertySections = new geom::property_section[ Geom.m_nPropertySections ];
        if( Geom.m_pPropertySections == NULL )
            x_throw( "Out of memory" );

        // Allocate properties
        Geom.m_nProperties = RawSettings.m_Properties.GetCount();
        Geom.m_pProperties = new geom::property[ Geom.m_nProperties ];
        if( Geom.m_pProperties == NULL )
            x_throw( "Out of memory" );
            
        // Compile property sections
        for( s32 iSection = 0; iSection < RawSettings.m_PropertySections.GetCount(); iSection++ )
        {
            // Lookup source + dest
            const raw_settings::property_section& SrcSection = RawSettings.m_PropertySections[iSection];
            geom::property_section&               DstSection = Geom.m_pPropertySections[iSection];
            
            // Setup dest
            DstSection.NameOffset  = m_Dictionary.Add( SrcSection.Name );
            DstSection.iProperty   = SrcSection.iProperty;
            DstSection.nProperties = SrcSection.nProperties;
        }

        // Compile properties
        s32 iProperty;
        for( iProperty = 0; iProperty < RawSettings.m_Properties.GetCount(); iProperty++ )
        {
            // Lookup source + dest
            const raw_settings::property& SrcProperty = RawSettings.m_Properties[iProperty];
            geom::property&               DstProperty = Geom.m_pProperties[iProperty];

            // Setup
            DstProperty.NameOffset = m_Dictionary.Add( SrcProperty.Name );
            if( SrcProperty.Type == "FLOAT" )
            {
                DstProperty.Type        = geom::property::TYPE_FLOAT;
                DstProperty.Value.Float = x_atof( SrcProperty.Value );
            }                
            else if( SrcProperty.Type == "INTEGER" )
            {
                DstProperty.Type          = geom::property::TYPE_INTEGER;
                DstProperty.Value.Integer = x_atoi( SrcProperty.Value );
            }                
            else if( SrcProperty.Type == "ANGLE" )
            {
                DstProperty.Type        = geom::property::TYPE_ANGLE;
                DstProperty.Value.Float = DEG_TO_RAD( x_atof( SrcProperty.Value ) );
            }                
            else if( SrcProperty.Type == "STRING" )
            {
                DstProperty.Type        = geom::property::TYPE_STRING;
                DstProperty.Value.StringOffset = m_Dictionary.Add( SrcProperty.Value );
            }
            else
            {
                x_throw( xfs( "Unknow property type %s\nReferenced in settings file [%s]\n\n", SrcProperty.Type, pSettingsFile ) );
            }
        }            
        
        // Show compiled info?
        if( g_Verbose )
        {
            x_printf("\nProperties:%d\n", Geom.m_nProperties );
            for( s32 iSection = 0; iSection < Geom.m_nPropertySections; iSection++ )
            {
                // Lookup section info
                geom::property_section& Section = Geom.m_pPropertySections[iSection];
                const char* pSection = m_Dictionary.GetString( Section.NameOffset );
                x_printf("\n   Section:%s nProperties:%d\n\n", m_Dictionary.GetString( Section.NameOffset ), Section.nProperties );
                
                // Loop through all properties
                for( iProperty = 0; iProperty < Section.nProperties; iProperty++ )
                {
                    // Lookup property info
                    const geom::property& Property = Geom.m_pProperties[ Section.iProperty + iProperty ];
                    const char* pName = m_Dictionary.GetString( Property.NameOffset );
                    xstring Type;
                    xstring Value;
                    switch( Property.Type )
                    {
                    case geom::property::TYPE_FLOAT:
                        Type = "FLOAT";
                        Value.Format( "%f", Property.Value.Float );
                        break;
                    case geom::property::TYPE_ANGLE:
                        Type = "ANGLE";
                        Value.Format( "%f", RAD_TO_DEG( Property.Value.Angle ) );
                        break;
                    case geom::property::TYPE_INTEGER:
                        Type = "INTEGER";
                        Value.Format( "%d", Property.Value.Integer );
                        break;
                    case geom::property::TYPE_STRING:
                        Type  = "STRING";
                        Value = m_Dictionary.GetString( Property.Value.StringOffset );
                        break;
                    default:
                        x_throw( xfs( "Unknow property type %s\nReferenced in settings file [%s]\n\n", Type, pSettingsFile ) );
                        break;
                    }
                    
                    // Show info
                    x_printf( "       Name:%30s    Type:%6s    Value:%8s\n", pName, (const char*)Type, (const char*)Value );
                }
                x_printf("\n");
            }                                              
        }
    }
}

//=============================================================================

