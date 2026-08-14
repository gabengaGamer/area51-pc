
#ifndef FILEIO_HPP
#define FILEIO_HPP

//=========================================================================
// INCLUDES
//=========================================================================
#include "mem_stream.hpp"
#include "x_files.hpp"

#if defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
#define FILEIO_TRANSLATE_PTRS 1
#else
#define FILEIO_TRANSLATE_PTRS 0
#endif

//=========================================================================
// FILE SYSTEM
//=========================================================================
class fileio
{
public:

    fileio( void ) : m_bStatic(TRUE), m_pWrite(NULL), m_bReading(FALSE) {}

    struct ref
    {
        s32     OffSet;         // Byte offset where the pointer lives
        s32     Count;          // Count of entries that this pointer is pointing to
        s32     PointingAT;     // What part of the file is this pointer pointing to
        u32     Flags;          // Flags for the miscelaneous stuff
    };

    struct resolve
    {
        s32     nPointers;
        ref*    pTable;
        byte*   pStatic;
        byte*   pDynamic;
    };

    template< class T >
    void Save( const char* pFileName, T& Object, xbool bEndianSwap )
    {
        X_FILE* Fp = x_fopen( pFileName, "wb" );
        if( Fp == NULL )
            x_throw( "Unable to save file");

        Save( Fp, Object, bEndianSwap );

        x_fclose( Fp );
    }


    template< class T >
    void Save( X_FILE* Fp, T& Object, xbool bEndianSwap )
    {
        ASSERT( Fp );
        m_Fp = Fp;

        m_pWrite = new writting;
        ASSERT( m_pWrite );

        m_bStatic   = TRUE;
        m_ClassPos  = 0;
        m_pClass    = (byte*)((void*)&Object);
        m_ClassSize = sizeof(Object);

        GetW().Preallocate( m_ClassSize, TRUE );

        Object.FileIO( *this );

        SaveFile();

        delete m_pWrite;
        m_pWrite = NULL;

        (void)bEndianSwap;
    }

    inline
    resolve* PreLoad( X_FILE* Fp )
    {
        resolve*     pResolve = NULL;
        file         File;
        s32          i;

        i = x_fread( &File, 1, sizeof(File), Fp );

        if( File.Signature != 0x56656e49 )
            x_throw( "Unkown file" );

        byte* pData    = new byte[ File.nStatic ];
        if( pData == NULL )
            x_throw( "Out of meory");
        x_fread( pData, 1, File.nStatic, Fp );

        byte* pDynamic = new byte[ File.nDynamic + sizeof(resolve) ];
        if( pDynamic == NULL )
        {
            delete[]pData;
            x_throw( "Out of meory");
        }
        x_fread( pDynamic, 1, File.nDynamic, Fp );

        pResolve            = (resolve*)&pDynamic[ File.nDynamic ];
        pResolve->nPointers = File.nTable;
        pResolve->pTable    = (ref*)&pData[ File.nStatic - (File.nTable * sizeof(ref)) ];
        pResolve->pStatic   = pData;
        pResolve->pDynamic  = pDynamic;

        return pResolve;
    }

#if !FILEIO_TRANSLATE_PTRS

    template< class T > inline
    void Resolved( resolve* pResolve, T*& pObject )
    {
        s32 i;
        for( i=0; i<pResolve->nPointers; i++ )
        {
            const ref& Ref = pResolve->pTable[i];
            switch( Ref.Flags )
            {
            case 3:
                *((void**)&pResolve->pStatic[ Ref.OffSet ])    = &pResolve->pStatic[ Ref.PointingAT ];
                break;
            case 0:
                *((void**)&pResolve->pDynamic[ Ref.OffSet ])   = &pResolve->pDynamic[ Ref.PointingAT ];
                break;
            case 1:
                *((void**)&pResolve->pStatic[ Ref.OffSet ])    = &pResolve->pDynamic[ Ref.PointingAT ];
                break;
            };
        }

        pObject = xConstruct( (T*)pResolve->pStatic, *this);

        delete[]pResolve->pDynamic;
    }

#else // FILEIO_TRANSLATE_PTRS

    template< class T > inline
    void Resolved( resolve* pResolve, T*& pObject )
    {
        m_bReading     = TRUE;
        m_pStaticBlob  = pResolve->pStatic;
        m_pDynamicBlob = pResolve->pDynamic;
        m_pRefTable    = pResolve->pTable;
        m_nRefTable    = pResolve->nPointers;

        // Pass 1 measures the native size of the graph, pass 2 builds it.
        m_bMeasure    = TRUE;
        m_RefIdx      = 0;
        m_BlobUsed    = BlobAlign( (s32)sizeof(T) );
        m_pReadRegion = m_pStaticBlob;
        {
            T* pTmp = (T*)x_malloc( sizeof(T) );
            ASSERT( pTmp );
            ReadStruct( pTmp, 0 );
            x_free( pTmp );
        }
        s32 Total = m_BlobUsed;

        // new[] (not x_malloc) to pair with the loaders' "delete pObject".
        m_pBlob       = new byte[ Total ];
        ASSERT( m_pBlob );
        m_bMeasure    = FALSE;
        m_RefIdx      = 0;
        m_BlobUsed    = BlobAlign( (s32)sizeof(T) );
        m_pReadRegion = m_pStaticBlob;
        ReadStruct( (T*)m_pBlob, 0 );

        if( m_BlobUsed != Total )
            x_DebugMsg( "FILEIO_OVERFLOW sizeofT=%d measure=%d fill=%d\n", (s32)sizeof(T), Total, m_BlobUsed );

        pObject = xConstruct( (T*)m_pBlob, *this );

        delete[] pResolve->pStatic;
        delete[] pResolve->pDynamic;

        m_bReading = FALSE;
    }

#endif // FILEIO_TRANSLATE_PTRS

    template< class T > inline
    void Load( X_FILE* Fp, T*& pObject )
    {
        resolve* pResolve = PreLoad( Fp );
        Resolved( pResolve, pObject );
    }

    template< class T > inline
    void Load( const char* pFileName, T*& pObject )
    {
        X_FILE* Fp = x_fopen( pFileName, "rb" );
        if( Fp == NULL )
            x_throw( xfs("Unable to open file %s", pFileName) );

        Load( Fp, pObject );

        x_fclose( Fp );
        Fp = NULL;
    }

    template< class T >
    void Static( T& A )
    {
        if( FILEIO_TRANSLATE_PTRS && m_bReading )
        {
            Handle( A );
            return;
        }

        byte* pA = (byte*)((void*)&A);
        ASSERTS( IsLocalVariable( pA ), "The variable must be a member of the class" );

        GetW().SeekPos( m_ClassPos + ComputeLocalOffset( pA ) );

        // IF YOU GET AN ERROR HERE WHILE LINKING CHANCES ARE YOU ARE TRYING TO PASS
        // A POINTER WHEN YOU SHOULD USE --- Static( pPtr, Count ) ---
        Handle( A );
    }

    template< class T >
    void StaticEnum( T& A )
    {
        if( sizeof(A) == 1 ) Static( *((u8*)&A)  );
        if( sizeof(A) == 2 ) Static( *((u16*)&A) );
        if( sizeof(A) == 4 ) Static( *((u32*)&A) );
        if( sizeof(A) > 4  ) ASSERT( 0 );
    }

    template< class T >
    void Static( T& A, s32 Count )          { Array( A, Count, TRUE );  }

    template< class T >
    void Dynamic( T*& A, s32 Count = 1 )    { Array( A, Count, FALSE ); }


    //=====================================================================
    // PRIVATE
    //=====================================================================
protected:

    struct writting
    {
        mem_stream  Static;
        mem_stream  Table;
        mem_stream  Dynamic;
    };

    struct file
    {
        u32 Signature;
        s32 Version;
        s32 nStatic;
        s32 nTable;
        s32 nDynamic;
    };

    //=====================================================================
    void SaveFile( void )
    {
        file File;

        s32 Alignment = m_pWrite->Static.GetLength();
            Alignment = ALIGN_4( Alignment ) - Alignment;

        File.Signature = 0x56656e49;
        File.Version   = 1;
        File.nStatic   = m_pWrite->Static.GetLength() + Alignment + m_pWrite->Table.GetLength();
        File.nDynamic  = m_pWrite->Dynamic.GetLength();
        File.nTable    = m_pWrite->Table.GetLength() / sizeof(ref);

        x_fwrite( &File, 1, sizeof(File), m_Fp );
        m_pWrite->Static.Save ( m_Fp );

        for( s32 i=0; i<Alignment; i++ )
        {
            const char Pad='?';
            x_fwrite( &Pad, 1, 1, m_Fp );
        }

        m_pWrite->Table.Save  ( m_Fp );
        m_pWrite->Dynamic.Save( m_Fp );
    }

    //=====================================================================
    inline mem_stream& GetW( void ) const
    {
        ASSERT( m_pWrite );
        if( m_bStatic ) return m_pWrite->Static;
        return m_pWrite->Dynamic;
    }

    inline mem_stream& GetTable( void ) const
    {
        ASSERT( m_pWrite );
        return m_pWrite->Table;
    }

    inline xbool IsLocalVariable( byte* pRange )
    {
        return (pRange >= m_pClass) && (pRange < ( m_pClass + m_ClassSize ));
    }

    inline s32 ComputeLocalOffset( u8* pItem )
    {
        ASSERT( IsLocalVariable( pItem ) );
        return (s32)(pItem - m_pClass);
    }

    //=====================================================================
    //=====================================================================
    static inline s32 BlobAlign( s32 v ) { return (v + 15) & ~15; }
    static inline s32 RdAlign( s32 v, s32 a ) { return (v + (a-1)) & ~(a-1); }

    template< class E > static inline s32 DiskAlignOf( void )
    {
        s32 a = (s32)__alignof(E);
        return (a == 8) ? 4 : a;
    }

    inline byte* blobAlloc( s32 Size )
    {
        s32 Off    = BlobAlign( m_BlobUsed );
        m_BlobUsed = Off + Size;
        return m_bMeasure ? NULL : (m_pBlob + Off);
    }

    template< class T > inline s32 DiskStructSize( void )
    {
        s32 trail  = (s32)sizeof(T) - m_NatCur;
        s32 excess = (s32)__alignof(T) - DiskAlignOf<T>();
        if( excess > trail ) excess = trail;
        if( excess < 0 )     excess = 0;
        return RdAlign( m_DiskCur + trail - excess, DiskAlignOf<T>() );
    }

    // Read a leaf value (no embedded pointers, so disk and native size match).
    inline void rdLeaf( void* pStore, s32 Size, s32 Align )
    {
        s32 NatOff = (s32)( (byte*)pStore - m_pClass );

        if( NatOff < m_NatCur )
        {
            x_memcpy( pStore, m_pReadRegion + m_DiskBase + NatOff, Size );
            return;
        }

        m_NatCur = RdAlign( m_NatCur, Align );
        if( NatOff > m_NatCur ) { m_DiskCur += NatOff - m_NatCur; m_NatCur = NatOff; }
        m_DiskCur = RdAlign( m_DiskCur, Align );
        x_memcpy( pStore, m_pReadRegion + m_DiskBase + m_DiskCur, Size );
        m_DiskCur += Size;
        m_NatCur  += Size;
    }

    template< class T >
    s32 ReadStruct( T* pStruct, s32 DiskBase )
    {
        byte* svClass = m_pClass; s32 svBase = m_DiskBase;
        s32   svNat   = m_NatCur; s32 svDisk = m_DiskCur;

        m_pClass = (byte*)pStruct; m_DiskBase = DiskBase;
        m_NatCur = 0; m_DiskCur = 0;
        pStruct->FileIO( *this );
        s32 Size = DiskStructSize<T>();

        m_pClass = svClass; m_DiskBase = svBase;
        m_NatCur = svNat; m_DiskCur = svDisk;
        return Size;
    }

    template< class E >
    s32 ReadElement( E* pElem, s32 DiskBase )
    {
        byte* svClass = m_pClass; s32 svBase = m_DiskBase;
        s32   svNat   = m_NatCur; s32 svDisk = m_DiskCur;

        m_pClass = (byte*)pElem; m_DiskBase = DiskBase;
        m_NatCur = 0; m_DiskCur = 0;
        Handle( *pElem );
        s32 Size = DiskStructSize<E>();

        m_pClass = svClass; m_DiskBase = svBase;
        m_NatCur = svNat; m_DiskCur = svDisk;
        return Size;
    }

    //=====================================================================
    template< class T >
    void Handle( T& A )
    {
        if( FILEIO_TRANSLATE_PTRS && m_bReading )
        {
            s32 NatOff = (s32)( (byte*)&A - m_pClass );
            m_NatCur = RdAlign( m_NatCur, (s32)__alignof(T) );
            if( NatOff > m_NatCur ) { m_DiskCur += NatOff - m_NatCur; m_NatCur = NatOff; }
            m_DiskCur = RdAlign( m_DiskCur, DiskAlignOf<T>() );

            s32 Size = ReadStruct( &A, m_DiskBase + m_DiskCur );

            m_DiskCur += Size;
            m_NatCur  += (s32)sizeof(T);
            return;
        }

        // TODO: Add support for complex reading
        if( m_pWrite == NULL )
            return;

        fileio File(*this);

        File.m_ClassPos     = GetW().Tell();
        File.m_pClass       = (byte*)&A;
        File.m_ClassSize    = sizeof( A );
        A.FileIO( File );

        GetW().SeekPos( File.m_ClassPos + sizeof( A ));
    }

    //=====================================================================
    void Handle( char& A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(char),1);return;} GetW().Write( &A, sizeof(char) );  }
    void Handle( s8&   A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(s8),  1);return;} GetW().Write( &A, sizeof(s8) );    }
    void Handle( s16&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(s16), 2);return;} GetW().Write( &A, sizeof(s16) );   }
    void Handle( s32&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(s32), 4);return;} GetW().Write( &A, sizeof(s32) );   }
    void Handle( s64&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(s64), 8);return;} GetW().Write( &A, sizeof(s64) );   }

    void Handle( u8&   A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(u8),  1);return;} GetW().Write( &A, sizeof(u8) );    }
    void Handle( u16&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(u16), 2);return;} GetW().Write( &A, sizeof(u16) );   }
    void Handle( u32&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(u32), 4);return;} GetW().Write( &A, sizeof(u32) );   }
    void Handle( u64&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(u64), 8);return;} GetW().Write( &A, sizeof(u64) );   }

    void Handle( f32&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(f32), 4);return;} GetW().Write( &A, sizeof(f32) );   }
    void Handle( f64&  A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(f64), 8);return;} GetW().Write( &A, sizeof(f64) );   }

    void Handle( vector3&    A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(vector3),   (s32)__alignof(vector3)   );return;} GetW().Write( &A, sizeof(vector3)    ); }
    void Handle( vector3p&   A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(vector3p),  (s32)__alignof(vector3p)  );return;} GetW().Write( &A, sizeof(vector3p)   ); }
    void Handle( bbox&       A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(bbox),      (s32)__alignof(bbox)      );return;} GetW().Write( &A, sizeof(bbox)       ); }
    void Handle( xcolor&     A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(xcolor),    (s32)__alignof(xcolor)    );return;} GetW().Write( &A, sizeof(xcolor)     ); }
    void Handle( vector2&    A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(vector2),   (s32)__alignof(vector2)   );return;} GetW().Write( &A, sizeof(vector2)    ); }
    void Handle( vector4&    A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(vector4),   (s32)__alignof(vector4)   );return;} GetW().Write( &A, sizeof(vector4)    ); }
    void Handle( quaternion& A ) { if(FILEIO_TRANSLATE_PTRS&&m_bReading){rdLeaf(&A,sizeof(quaternion),(s32)__alignof(quaternion));return;} GetW().Write( &A, sizeof(quaternion) ); }

    //=====================================================================
    template< class E >
    void Array( E*& A, s32 Count, xbool bStatic )
    {
        ASSERT( Count >= 0 );
        HandlePtr( A, Count, bStatic );
    }

    //=====================================================================
    template< class E, s32 N >
    void Array( E (&A)[N], s32 Count, xbool bStatic )
    {
        ASSERT( Count >= 0 );

        if( FILEIO_TRANSLATE_PTRS && m_bReading )
        {
            for( s32 i=0; i<Count; i++ )
                Handle( A[i] );
            return;
        }

        ASSERTS( bStatic == TRUE, "All local arrays must be static" );

        GetW().SeekPos( m_ClassPos + ComputeLocalOffset( (byte*)((void*)A) ) );
        GetW().Preallocate( sizeof(A) );

        for( s32 i=0; i<Count; i++ )
            Handle( A[i] );
    }

    //=====================================================================
    template< class E >
    void HandlePtr( E*& A, s32 Count, xbool bStatic )
    {
        if( FILEIO_TRANSLATE_PTRS && m_bReading )
        {
            // Account for the 4-byte on-disk pointer slot in the parent struct.
            s32 NatOff = (s32)( (byte*)&A - m_pClass );
            m_NatCur = RdAlign( m_NatCur, (s32)sizeof(void*) );
            if( NatOff > m_NatCur ) { m_DiskCur += NatOff - m_NatCur; m_NatCur = NatOff; }
            m_DiskCur  = RdAlign( m_DiskCur, 4 );
            m_DiskCur += 4;
            m_NatCur  += (s32)sizeof(void*);

            if( Count == 0 )
            {
                if( !m_bMeasure ) A = NULL;
                return;
            }

            const ref&  R       = m_pRefTable[ m_RefIdx++ ];
            const byte* pTarget = (R.Flags & 2) ? m_pStaticBlob : m_pDynamicBlob;

            E*    pArr = NULL;
            byte* pTmp = NULL;
            if( m_bMeasure )
            {
                blobAlloc( Count * (s32)sizeof(E) );
                pTmp = (byte*)x_malloc( sizeof(E) );
                pArr = (E*)pTmp;
            }
            else
            {
                pArr = (E*)blobAlloc( Count * (s32)sizeof(E) );
                A    = pArr;
            }

            const byte* SaveRegion = m_pReadRegion;
            m_pReadRegion = pTarget;

            // The first element reveals the on-disk stride; the rest follow it.
            s32 Stride = (s32)sizeof(E);
            for( s32 i=0; i<Count; i++ )
            {
                E*  pElem = m_bMeasure ? pArr : &pArr[i];
                s32 Size  = ReadElement( pElem, R.PointingAT + i * Stride );
                if( i == 0 && Size > 0 ) Stride = Size;
            }

            m_pReadRegion = SaveRegion;
            if( pTmp ) x_free( pTmp );
            return;
        }

        ref    Ref;
        byte* pA = (byte*)((void*)&A);
        ASSERTS( !((m_bStatic == FALSE) && (bStatic == TRUE)), "The parent of these structure is been save as dynamic. Not allow statics." );

        if( Count == 0 )
        {
            Static( *((s32*)(&A)) );
            return;
        }

        {
            if( bStatic )
            {
                m_pWrite->Static.GotoEnd();
                m_pWrite->Static.Preallocate32( sizeof(*A) * Count );
                Ref.PointingAT = m_pWrite->Static.Tell();
            }
            else
            {
                m_pWrite->Dynamic.GotoEnd();
                m_pWrite->Dynamic.Preallocate32( sizeof(*A) * Count );
                Ref.PointingAT = m_pWrite->Dynamic.Tell();
            }

            Ref.OffSet     = m_ClassPos + ComputeLocalOffset( pA );
            Ref.Count      = Count;
            Ref.Flags      = ((bStatic==TRUE)<<1) | ((m_bStatic==TRUE)<<0);

            GetTable().Write( &Ref, sizeof(Ref) );
        }

        xbool BackStatic = m_bStatic;
               m_bStatic = bStatic;

        ASSERT( Ref.PointingAT == GetW().Tell() );

        for( s32 i=0; i<Count; i++ )
            Handle( A[i] );

        m_bStatic = BackStatic;
    }

    template< class E, s32 N >
    void HandlePtr( E (&)[N], s32, xbool ) { ASSERT( 0 ); }

    xbool       m_bStatic;
    writting*   m_pWrite;
    X_FILE*     m_Fp;
    s32         m_ClassPos;
    byte*       m_pClass;
    s32         m_ClassSize;

    // read-mode state
    xbool        m_bReading;
    xbool        m_bMeasure;
    const byte*  m_pStaticBlob;
    const byte*  m_pDynamicBlob;
    const ref*   m_pRefTable;
    s32          m_nRefTable;
    s32          m_RefIdx;
    const byte*  m_pReadRegion;   // region being read (static or dynamic blob)
    s32          m_DiskBase;      // on-disk offset of the current struct
    s32          m_NatCur;        // cursor over the native struct layout
    s32          m_DiskCur;       // cursor over the on-disk struct layout
    byte*        m_pBlob;
    s32          m_BlobUsed;
};

#endif
