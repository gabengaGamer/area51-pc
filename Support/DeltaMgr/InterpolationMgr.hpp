#ifndef INTERPOLATION_MGR_HPP
#define INTERPOLATION_MGR_HPP

#include "Entropy.hpp"

class interpolation_mgr
{
public:
    enum clear_stage
    {
        CLEAR_STAGE_PER_VIEW,
        CLEAR_STAGE_END_FRAME
    };

    typedef void capture_fn( void );
    typedef void update_fn ( f32 Alpha );
    typedef void clear_fn  ( void );

    class provider
    {
    public:
                    provider( capture_fn* pCapture,
                              update_fn*  pUpdate,
                              clear_fn*   pClear,
                              clear_stage ClearStage,
                              s32         Priority );

    private:
        static  provider*&  GetHead     ( void );

        friend class interpolation_mgr;

        provider*           m_pNext;
        capture_fn*         m_pCapture;
        update_fn*          m_pUpdate;
        clear_fn*           m_pClear;
        clear_stage         m_ClearStage;
        s32                 m_Priority;
    };

            interpolation_mgr    ( void );

    void    Capture              ( void ) const;
    void    Update               ( f32 Alpha ) const;
    void    Clear                ( clear_stage Stage ) const;
};

extern interpolation_mgr g_InterpolationMgr;

#endif
