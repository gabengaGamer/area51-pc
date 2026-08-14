//==============================================================================
//
//  x_atomic_linux.cpp
//
//==============================================================================

#if !defined(TARGET_LINUX)
#error "This is only for the Linux target platform. Please check build exclusion rules"
#endif

#include "../x_atomic.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void x_AtomicInit( x_atomic_s32* pAtomic, s32 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicInit( x_atomic_u32* pAtomic, u32 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicInit( x_atomic_s64* pAtomic, s64 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicInit( x_atomic_u64* pAtomic, u64 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicInit( x_atomic_ptr* pAtomic, void* Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

s32 x_AtomicLoadRelaxed( const x_atomic_s32* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_RELAXED );
}

//==============================================================================

u32 x_AtomicLoadRelaxed( const x_atomic_u32* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_RELAXED );
}

//==============================================================================

s64 x_AtomicLoadRelaxed( const x_atomic_s64* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_RELAXED );
}

//==============================================================================

u64 x_AtomicLoadRelaxed( const x_atomic_u64* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_RELAXED );
}

//==============================================================================

void* x_AtomicLoadRelaxed( const x_atomic_ptr* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_RELAXED );
}

//==============================================================================

s32 x_AtomicLoadAcquire( const x_atomic_s32* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_ACQUIRE );
}

//==============================================================================

u32 x_AtomicLoadAcquire( const x_atomic_u32* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_ACQUIRE );
}

//==============================================================================

s64 x_AtomicLoadAcquire( const x_atomic_s64* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_ACQUIRE );
}

//==============================================================================

u64 x_AtomicLoadAcquire( const x_atomic_u64* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_ACQUIRE );
}

//==============================================================================

void* x_AtomicLoadAcquire( const x_atomic_ptr* pAtomic )
{
    return __atomic_load_n( &pAtomic->Value, __ATOMIC_ACQUIRE );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_s32* pAtomic, s32 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_u32* pAtomic, u32 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_s64* pAtomic, s64 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_u64* pAtomic, u64 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_ptr* pAtomic, void* Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELAXED );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_s32* pAtomic, s32 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELEASE );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_u32* pAtomic, u32 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELEASE );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_s64* pAtomic, s64 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELEASE );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_u64* pAtomic, u64 Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELEASE );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_ptr* pAtomic, void* Value )
{
    __atomic_store_n( &pAtomic->Value, Value, __ATOMIC_RELEASE );
}

//==============================================================================

s32 x_AtomicExchange( x_atomic_s32* pAtomic, s32 Value )
{
    return __atomic_exchange_n( &pAtomic->Value, Value, __ATOMIC_SEQ_CST );
}

//==============================================================================

u32 x_AtomicExchange( x_atomic_u32* pAtomic, u32 Value )
{
    return __atomic_exchange_n( &pAtomic->Value, Value, __ATOMIC_SEQ_CST );
}

//==============================================================================

s64 x_AtomicExchange( x_atomic_s64* pAtomic, s64 Value )
{
    return __atomic_exchange_n( &pAtomic->Value, Value, __ATOMIC_SEQ_CST );
}

//==============================================================================

u64 x_AtomicExchange( x_atomic_u64* pAtomic, u64 Value )
{
    return __atomic_exchange_n( &pAtomic->Value, Value, __ATOMIC_SEQ_CST );
}

//==============================================================================

void* x_AtomicExchange( x_atomic_ptr* pAtomic, void* Value )
{
    return __atomic_exchange_n( &pAtomic->Value, Value, __ATOMIC_SEQ_CST );
}

//==============================================================================

s32 x_AtomicCompareExchange( x_atomic_s32* pAtomic, s32 Exchange, s32 Comparand )
{
    s32 Expected = Comparand;
    __atomic_compare_exchange_n( &pAtomic->Value,
                                  &Expected,
                                  Exchange,
                                  false,
                                  __ATOMIC_SEQ_CST,
                                  __ATOMIC_SEQ_CST );
    return Expected;
}

//==============================================================================

u32 x_AtomicCompareExchange( x_atomic_u32* pAtomic, u32 Exchange, u32 Comparand )
{
    u32 Expected = Comparand;
    __atomic_compare_exchange_n( &pAtomic->Value,
                                  &Expected,
                                  Exchange,
                                  false,
                                  __ATOMIC_SEQ_CST,
                                  __ATOMIC_SEQ_CST );
    return Expected;
}

//==============================================================================

s64 x_AtomicCompareExchange( x_atomic_s64* pAtomic, s64 Exchange, s64 Comparand )
{
    s64 Expected = Comparand;
    __atomic_compare_exchange_n( &pAtomic->Value,
                                  &Expected,
                                  Exchange,
                                  false,
                                  __ATOMIC_SEQ_CST,
                                  __ATOMIC_SEQ_CST );
    return Expected;
}

//==============================================================================

u64 x_AtomicCompareExchange( x_atomic_u64* pAtomic, u64 Exchange, u64 Comparand )
{
    u64 Expected = Comparand;
    __atomic_compare_exchange_n( &pAtomic->Value,
                                  &Expected,
                                  Exchange,
                                  false,
                                  __ATOMIC_SEQ_CST,
                                  __ATOMIC_SEQ_CST );
    return Expected;
}

//==============================================================================

void* x_AtomicCompareExchange( x_atomic_ptr* pAtomic, void* Exchange, void* Comparand )
{
    void* Expected = Comparand;
    __atomic_compare_exchange_n( &pAtomic->Value,
                                  &Expected,
                                  Exchange,
                                  false,
                                  __ATOMIC_SEQ_CST,
                                  __ATOMIC_SEQ_CST );
    return Expected;
}

//==============================================================================

void x_AtomicFenceAcquire( void )
{
    __atomic_thread_fence( __ATOMIC_ACQUIRE );
}

//==============================================================================

void x_AtomicFenceRelease( void )
{
    __atomic_thread_fence( __ATOMIC_RELEASE );
}

//==============================================================================

void x_AtomicFenceFull( void )
{
    __atomic_thread_fence( __ATOMIC_SEQ_CST );
}
