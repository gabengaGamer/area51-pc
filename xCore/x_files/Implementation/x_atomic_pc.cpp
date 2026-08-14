//==============================================================================
//
//  x_atomic_pc.cpp
//
//==============================================================================

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

#include "../x_atomic.hpp"
#include <windows.h>

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
volatile LONG* x_atomic_AsLong( volatile s32* pValue )
{
    return (volatile LONG*)pValue;
}

//==============================================================================

static 
volatile LONG* x_atomic_AsLong( volatile u32* pValue )
{
    return (volatile LONG*)pValue;
}

//==============================================================================

static 
volatile LONG64* x_atomic_AsLong64( volatile s64* pValue )
{
    return (volatile LONG64*)pValue;
}

//==============================================================================

static 
volatile LONG64* x_atomic_AsLong64( volatile u64* pValue )
{
    return (volatile LONG64*)pValue;
}

//==============================================================================

static 
void* volatile* x_atomic_AsPointer( void* volatile* pValue )
{
    return (void* volatile*)pValue;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void x_AtomicInit( x_atomic_s32* pAtomic, s32 Value )
{
    pAtomic->Value = Value;
}

//==============================================================================

void x_AtomicInit( x_atomic_u32* pAtomic, u32 Value )
{
    pAtomic->Value = Value;
}

//==============================================================================

void x_AtomicInit( x_atomic_s64* pAtomic, s64 Value )
{
    InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void x_AtomicInit( x_atomic_u64* pAtomic, u64 Value )
{
    InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void x_AtomicInit( x_atomic_ptr* pAtomic, void* Value )
{
    pAtomic->Value = Value;
}

//==============================================================================

s32 x_AtomicLoadRelaxed( const x_atomic_s32* pAtomic )
{
    return pAtomic->Value;
}

//==============================================================================

u32 x_AtomicLoadRelaxed( const x_atomic_u32* pAtomic )
{
    return pAtomic->Value;
}

//==============================================================================

s64 x_AtomicLoadRelaxed( const x_atomic_s64* pAtomic )
{
    return (s64)InterlockedCompareExchange64( x_atomic_AsLong64( (volatile s64*)&pAtomic->Value ), 0, 0 );
}

//==============================================================================

u64 x_AtomicLoadRelaxed( const x_atomic_u64* pAtomic )
{
    return (u64)InterlockedCompareExchange64( x_atomic_AsLong64( (volatile u64*)&pAtomic->Value ), 0, 0 );
}

//==============================================================================

void* x_AtomicLoadRelaxed( const x_atomic_ptr* pAtomic )
{
    return pAtomic->Value;
}

//==============================================================================

s32 x_AtomicLoadAcquire( const x_atomic_s32* pAtomic )
{
    return (s32)InterlockedCompareExchange( x_atomic_AsLong( (volatile s32*)&pAtomic->Value ), 0, 0 );
}

//==============================================================================

u32 x_AtomicLoadAcquire( const x_atomic_u32* pAtomic )
{
    return (u32)InterlockedCompareExchange( x_atomic_AsLong( (volatile u32*)&pAtomic->Value ), 0, 0 );
}

//==============================================================================

s64 x_AtomicLoadAcquire( const x_atomic_s64* pAtomic )
{
    return (s64)InterlockedCompareExchange64( x_atomic_AsLong64( (volatile s64*)&pAtomic->Value ), 0, 0 );
}

//==============================================================================

u64 x_AtomicLoadAcquire( const x_atomic_u64* pAtomic )
{
    return (u64)InterlockedCompareExchange64( x_atomic_AsLong64( (volatile u64*)&pAtomic->Value ), 0, 0 );
}

//==============================================================================

void* x_AtomicLoadAcquire( const x_atomic_ptr* pAtomic )
{
    return InterlockedCompareExchangePointer( x_atomic_AsPointer( (void* volatile*)&pAtomic->Value ), NULL, NULL );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_s32* pAtomic, s32 Value )
{
    pAtomic->Value = Value;
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_u32* pAtomic, u32 Value )
{
    pAtomic->Value = Value;
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_s64* pAtomic, s64 Value )
{
    InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_u64* pAtomic, u64 Value )
{
    InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void x_AtomicStoreRelaxed( x_atomic_ptr* pAtomic, void* Value )
{
    pAtomic->Value = Value;
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_s32* pAtomic, s32 Value )
{
    InterlockedExchange( x_atomic_AsLong( &pAtomic->Value ), (LONG)Value );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_u32* pAtomic, u32 Value )
{
    InterlockedExchange( x_atomic_AsLong( &pAtomic->Value ), (LONG)Value );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_s64* pAtomic, s64 Value )
{
    InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_u64* pAtomic, u64 Value )
{
    InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void x_AtomicStoreRelease( x_atomic_ptr* pAtomic, void* Value )
{
    InterlockedExchangePointer( x_atomic_AsPointer( &pAtomic->Value ), Value );
}

//==============================================================================

s32 x_AtomicExchange( x_atomic_s32* pAtomic, s32 Value )
{
    return (s32)InterlockedExchange( x_atomic_AsLong( &pAtomic->Value ), (LONG)Value );
}

//==============================================================================

u32 x_AtomicExchange( x_atomic_u32* pAtomic, u32 Value )
{
    return (u32)InterlockedExchange( x_atomic_AsLong( &pAtomic->Value ), (LONG)Value );
}

//==============================================================================

s64 x_AtomicExchange( x_atomic_s64* pAtomic, s64 Value )
{
    return (s64)InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

u64 x_AtomicExchange( x_atomic_u64* pAtomic, u64 Value )
{
    return (u64)InterlockedExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Value );
}

//==============================================================================

void* x_AtomicExchange( x_atomic_ptr* pAtomic, void* Value )
{
    return InterlockedExchangePointer( x_atomic_AsPointer( &pAtomic->Value ), Value );
}

//==============================================================================

s32 x_AtomicCompareExchange( x_atomic_s32* pAtomic, s32 Exchange, s32 Comparand )
{
    return (s32)InterlockedCompareExchange( x_atomic_AsLong( &pAtomic->Value ), (LONG)Exchange, (LONG)Comparand );
}

//==============================================================================

u32 x_AtomicCompareExchange( x_atomic_u32* pAtomic, u32 Exchange, u32 Comparand )
{
    return (u32)InterlockedCompareExchange( x_atomic_AsLong( &pAtomic->Value ), (LONG)Exchange, (LONG)Comparand );
}

//==============================================================================

s64 x_AtomicCompareExchange( x_atomic_s64* pAtomic, s64 Exchange, s64 Comparand )
{
    return (s64)InterlockedCompareExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Exchange, (LONG64)Comparand );
}

//==============================================================================

u64 x_AtomicCompareExchange( x_atomic_u64* pAtomic, u64 Exchange, u64 Comparand )
{
    return (u64)InterlockedCompareExchange64( x_atomic_AsLong64( &pAtomic->Value ), (LONG64)Exchange, (LONG64)Comparand );
}

//==============================================================================

void* x_AtomicCompareExchange( x_atomic_ptr* pAtomic, void* Exchange, void* Comparand )
{
    return InterlockedCompareExchangePointer( x_atomic_AsPointer( &pAtomic->Value ), Exchange, Comparand );
}

//==============================================================================

void x_AtomicFenceAcquire( void )
{
    MemoryBarrier();
}

//==============================================================================

void x_AtomicFenceRelease( void )
{
    MemoryBarrier();
}

//==============================================================================

void x_AtomicFenceFull( void )
{
    MemoryBarrier();
}