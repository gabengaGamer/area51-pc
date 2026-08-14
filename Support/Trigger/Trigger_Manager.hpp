///////////////////////////////////////////////////////////////////////////////
//
//  Trigger_Manager.hpp
//
//
///////////////////////////////////////////////////////////////////////////////


#ifndef _TRIGGER_MANAGER_
#define _TRIGGER_MANAGER_

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "x_array.hpp"
#include "x_profile.hpp"

#include "MiscUtils/Property.hpp"
#include "../Support/Trigger/Trigger_Object.hpp"

//=========================================================================
// TRIGGER_MNGR
//=========================================================================

class trigger_mngr
{
public:
                        trigger_mngr        ( );

    void                OnUpdate            ( f32 DeltaTime );

    void                RegisterTrigger     ( trigger_object &  rTrigger );    //called by the trigger object on OnInit
    void                UnregisterTrigger   ( trigger_object &  rTrigger );    //called to remove triggers from the mngr..
    void                TriggerSleep        ( trigger_object &  rTrigger );
    void                TriggerAwake        ( trigger_object &  rTrigger );

protected:
  
    enum trigger_class
    {
        INVALID_TRIGGER_CLASS = -1,

        TRIGGER_NO_UPDATE,
        TRIGGER_UPDATE,

        TRIGGER_CLASS_END
    };

    trigger_object*     GetTriggerPtr       (  guid TriggerGuid );

    guid                     m_ClassList[TRIGGER_CLASS_END];        // List of Triggers
};

extern trigger_mngr g_TriggerMgr;

#endif  //_TRIGGER_MANAGER_
