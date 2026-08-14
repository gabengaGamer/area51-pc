//==============================================================================
//
//  x_atomic.hpp
//
//==============================================================================

#ifndef X_ATOMIC_HPP
#define X_ATOMIC_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct x_atomic_s32
{
    volatile s32 Value;
};

//------------------------------------------------------------------------------

struct x_atomic_u32
{
    volatile u32 Value;
};

//------------------------------------------------------------------------------

struct PC_ALIGNMENT(8) x_atomic_s64
{
    volatile s64 Value;
};

//------------------------------------------------------------------------------

struct PC_ALIGNMENT(8) x_atomic_u64
{
    volatile u64 Value;
};

//------------------------------------------------------------------------------

struct x_atomic_ptr
{
    void* volatile Value;
};

//==============================================================================
//  DEFINES
//==============================================================================

#define X_ATOMIC_S32_INIT(v)    { (s32)(v) }
#define X_ATOMIC_U32_INIT(v)    { (u32)(v) }
#define X_ATOMIC_S64_INIT(v)    { (s64)(v) }
#define X_ATOMIC_U64_INIT(v)    { (u64)(v) }
#define X_ATOMIC_PTR_INIT(v)    { (void*)(v) }

// Use Acquire loads with Release stores for cross-thread publication. The
// Relaxed calls only guarantee atomic access to the stored value.
// x_AtomicCompareExchange returns the previous value.

//==============================================================================
//  FUNCTIONS
//==============================================================================

    void        x_AtomicInit           ( x_atomic_s32* pAtomic, s32 Value );
    void        x_AtomicInit           ( x_atomic_u32* pAtomic, u32 Value );
    void        x_AtomicInit           ( x_atomic_s64* pAtomic, s64 Value );
    void        x_AtomicInit           ( x_atomic_u64* pAtomic, u64 Value );
    void        x_AtomicInit           ( x_atomic_ptr* pAtomic, void* Value );

    s32         x_AtomicLoadRelaxed    ( const x_atomic_s32* pAtomic );
    u32         x_AtomicLoadRelaxed    ( const x_atomic_u32* pAtomic );
    s64         x_AtomicLoadRelaxed    ( const x_atomic_s64* pAtomic );
    u64         x_AtomicLoadRelaxed    ( const x_atomic_u64* pAtomic );
    void*       x_AtomicLoadRelaxed    ( const x_atomic_ptr* pAtomic );

    s32         x_AtomicLoadAcquire    ( const x_atomic_s32* pAtomic );
    u32         x_AtomicLoadAcquire    ( const x_atomic_u32* pAtomic );
    s64         x_AtomicLoadAcquire    ( const x_atomic_s64* pAtomic );
    u64         x_AtomicLoadAcquire    ( const x_atomic_u64* pAtomic );
    void*       x_AtomicLoadAcquire    ( const x_atomic_ptr* pAtomic );

    void        x_AtomicStoreRelaxed   ( x_atomic_s32* pAtomic, s32 Value );
    void        x_AtomicStoreRelaxed   ( x_atomic_u32* pAtomic, u32 Value );
    void        x_AtomicStoreRelaxed   ( x_atomic_s64* pAtomic, s64 Value );
    void        x_AtomicStoreRelaxed   ( x_atomic_u64* pAtomic, u64 Value );
    void        x_AtomicStoreRelaxed   ( x_atomic_ptr* pAtomic, void* Value );

    void        x_AtomicStoreRelease   ( x_atomic_s32* pAtomic, s32 Value );
    void        x_AtomicStoreRelease   ( x_atomic_u32* pAtomic, u32 Value );
    void        x_AtomicStoreRelease   ( x_atomic_s64* pAtomic, s64 Value );
    void        x_AtomicStoreRelease   ( x_atomic_u64* pAtomic, u64 Value );
    void        x_AtomicStoreRelease   ( x_atomic_ptr* pAtomic, void* Value );

    s32         x_AtomicExchange       ( x_atomic_s32* pAtomic, s32 Value );
    u32         x_AtomicExchange       ( x_atomic_u32* pAtomic, u32 Value );
    s64         x_AtomicExchange       ( x_atomic_s64* pAtomic, s64 Value );
    u64         x_AtomicExchange       ( x_atomic_u64* pAtomic, u64 Value );
    void*       x_AtomicExchange       ( x_atomic_ptr* pAtomic, void* Value );

    s32         x_AtomicCompareExchange( x_atomic_s32* pAtomic, s32 Exchange, s32 Comparand );
    u32         x_AtomicCompareExchange( x_atomic_u32* pAtomic, u32 Exchange, u32 Comparand );
    s64         x_AtomicCompareExchange( x_atomic_s64* pAtomic, s64 Exchange, s64 Comparand );
    u64         x_AtomicCompareExchange( x_atomic_u64* pAtomic, u64 Exchange, u64 Comparand );
    void*       x_AtomicCompareExchange( x_atomic_ptr* pAtomic, void* Exchange, void* Comparand );

    void        x_AtomicFenceAcquire   ( void );
    void        x_AtomicFenceRelease   ( void );
    void        x_AtomicFenceFull      ( void );

//==============================================================================
#endif // X_ATOMIC_HPP
//==============================================================================
