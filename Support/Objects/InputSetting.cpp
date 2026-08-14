//==============================================================================
//
//  InputSetting.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "InputSetting.hpp"

//=========================================================================
// OBJECT DESCRIPTION
//=========================================================================
static struct input_setting_desc : public object_desc
{
    input_setting_desc( void ) : object_desc( 
        object::TYPE_INPUT_SETTINGS, 
        "Input Setting", 
        "SYSTEM",

        object::ATTR_NULL,

        FLAGS_GENERIC_EDITOR_CREATE ) {}         

    //---------------------------------------------------------------------

    virtual object* Create          ( void )
    {
        return new input_setting;
    }

    //---------------------------------------------------------------------

#ifdef X_EDITOR

    virtual s32 OnEditorRender( object& Object ) const
    { 
        object_desc::OnEditorRender( Object );
        return static_cast<s32>( EditorIcon::InputSettings ); 
    }

#endif // X_EDITOR

} s_InputSetting_Desc;

//=========================================================================

const object_desc& input_setting::GetTypeDesc( void ) const
{
    return s_InputSetting_Desc;
}

//=========================================================================

const object_desc& input_setting::GetObjectType( void )
{
    return s_InputSetting_Desc;
}


//==============================================================================
// InputSetting
//==============================================================================

input_setting::input_setting ( void )
{
}

//==============================================================================

input_setting::~input_setting ( void )
{
}

//==============================================================================
                                                    
void input_setting::OnInit( void )
{
}
