//==============================================================================
//
// dfs.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "dfs.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

dfs_header* dfs_InitHeaderFromRawPtr( void* pRawHeaderData  )
{
    dfs_header* pHeader = (dfs_header*)pRawHeaderData;

    // Endian swap the header...
    pHeader->Magic          = LITTLE_ENDIAN_32( pHeader->Magic          );
    pHeader->Version        = LITTLE_ENDIAN_32( pHeader->Version        );
    pHeader->SectorSize     = LITTLE_ENDIAN_32( pHeader->SectorSize     );
    pHeader->SplitSize      = LITTLE_ENDIAN_32( pHeader->SplitSize      );
    pHeader->nFiles         = LITTLE_ENDIAN_32( pHeader->nFiles         );
    pHeader->nSubFiles      = LITTLE_ENDIAN_32( pHeader->nSubFiles      );
    pHeader->StringsLength  = LITTLE_ENDIAN_32( pHeader->StringsLength  );
    pHeader->SubFileTableOffset = LITTLE_ENDIAN_32( pHeader->SubFileTableOffset );
    pHeader->FilesOffset        = LITTLE_ENDIAN_32( pHeader->FilesOffset        );
    pHeader->ChecksumsOffset    = LITTLE_ENDIAN_32( pHeader->ChecksumsOffset    );
    pHeader->StringsOffset      = LITTLE_ENDIAN_32( pHeader->StringsOffset      );

    // Make sure its valid!
    if( (pHeader->Version == DFS_VERSION) )
    {
        dfs_file* pEntry;
        s32       i;

        // Byte swap the filesize / checksum index table.
        dfs_subfile* pTable = dfs_GetSubFileTable( pHeader );
        for( i=0 ; i<pHeader->nSubFiles ; i++ )
        {
            pTable[i].Offset        = LITTLE_ENDIAN_32( pTable[i].Offset );
            pTable[i].ChecksumIndex = LITTLE_ENDIAN_32( pTable[i].ChecksumIndex );
        }

        // Byte swap the file entries.
        for( i=0, pEntry=dfs_GetFiles( pHeader ) ; i<pHeader->nFiles ; i++,pEntry++ )
        {
            // Byte swap the 32-bit values.
            pEntry->FileNameOffset1 = LITTLE_ENDIAN_32( pEntry->FileNameOffset1 );
            pEntry->FileNameOffset2 = LITTLE_ENDIAN_32( pEntry->FileNameOffset2 );
            pEntry->PathNameOffset  = LITTLE_ENDIAN_32( pEntry->PathNameOffset  );
            pEntry->ExtNameOffset   = LITTLE_ENDIAN_32( pEntry->ExtNameOffset   );

            // Byte swap the 32-bit values.
            pEntry->DataOffset = LITTLE_ENDIAN_32( pEntry->DataOffset );
            pEntry->Length     = LITTLE_ENDIAN_32( pEntry->Length     );  
        }

        // Woot!
        return pHeader;
    }

    return NULL;
}

//==============================================================================

void dfs_DumpFileListing( const dfs_header* pHeader, const char* pFileName )
{
    X_FILE* f;
    f = x_fopen( pFileName, "w+t" );
    if( f )
    {
        const dfs_file* pEntry   = dfs_GetFiles( pHeader );
        const char*     pStrings = dfs_GetStrings( pHeader );

        for( s32 i=0 ; i<pHeader->nFiles ; i++, pEntry++ )
        {
            x_fprintf( f,"%8d\t%8d\t%8d\t%s\t%s%s\t%s\n",
                i,
                pEntry->Length,
                pEntry->DataOffset,
                pStrings + pEntry->PathNameOffset,
                pStrings + pEntry->FileNameOffset1,
                pStrings + pEntry->FileNameOffset2,
                pStrings + pEntry->ExtNameOffset);
        }

        x_fclose( f );
    }
}

//==============================================================================

void dfs_Extract( const xstring& dfsPathName, const xstring& outPath )
{
    // read in the dfs file
    X_FILE* fp = x_fopen(dfsPathName,"rb");
    if( !fp )
        return;

    s32 FSize = x_flength(fp);
    if( FSize < (s32)sizeof(dfs_header) )
    {
        x_fclose(fp);
        return;
    }

    byte* pDFS = (byte*)x_malloc(FSize);
    if(!pDFS)
    {
        x_fclose(fp);
        return;
    }

    x_fread( pDFS, FSize, 1, fp );
    x_fclose(fp);

    dfs_header* pHeader = dfs_InitHeaderFromRawPtr( pDFS );
    if( !pHeader )
    {
        x_free( pDFS );
        return;
    }

    dfs_file*   pEntry  = dfs_GetFiles( pHeader );
    const char* pStrings = dfs_GetStrings( pHeader );

    // open the associated sub files
    char DfsSubFile[X_MAX_PATH];
    char DfsDrive[X_MAX_DRIVE];
    char DfsPath[X_MAX_PATH];
    char DfsName[X_MAX_FNAME];
    char DfsExt[X_MAX_EXT];
    x_splitpath((const char*)dfsPathName, DfsDrive, DfsPath, DfsName, DfsExt);

    X_FILE** pSf = (X_FILE**)x_malloc(pHeader->nSubFiles * sizeof(X_FILE*));
    ASSERT( pSf );

    // open all the sub files.
    for( s32 i=0 ; i < pHeader->nSubFiles ; i++ )
    {
        x_sprintf(DfsSubFile, "%s%s%s.%03d", DfsDrive, DfsPath, DfsName, i);

        pSf[i] = x_fopen(DfsSubFile, "rb");
        ASSERT( pSf[i] );
    }

    u32 bufferSize = 0;
    byte* pFiledata;

    // allocate for the largest file.
    for( s32 i=0 ; i<pHeader->nFiles ; i++, pEntry++ )
    {
        if( pEntry->Length > bufferSize )
        {
            bufferSize = pEntry->Length;
        }
    }

    pFiledata = (byte*)x_malloc(bufferSize);
    ASSERT(pFiledata);

    // extract files.
    pEntry  = dfs_GetFiles( pHeader );

    u32 sub = 0;    // start with the first sub file
    u32 subfileOffset = 0;  // the amount to take off from the dataoffset

    for( s32 i=0 ; i<pHeader->nFiles ; i++, pEntry++ )
    {
        char OutputFilePath[X_MAX_PATH];

        // get the output filename
        if ( !outPath.IsEmpty() )  // path override
        {
            x_sprintf( OutputFilePath, "%s%s%s%s",
                (const char*)outPath,
                pStrings + pEntry->FileNameOffset1,
                pStrings + pEntry->FileNameOffset2,
                pStrings + pEntry->ExtNameOffset);
        }
        else    // use original path
        {
            x_sprintf( OutputFilePath, "%s%s%s%s",
                pStrings + pEntry->PathNameOffset,
                pStrings + pEntry->FileNameOffset1,
                pStrings + pEntry->FileNameOffset2,
                pStrings + pEntry->ExtNameOffset);
        }

        // open the new file.
        fp = x_fopen(OutputFilePath, "wb");
        if (fp)
        {
            u32 offset = pEntry->DataOffset - subfileOffset;
            u32 length = pEntry->Length;
            u32 bytesRead = 0;

            if (pSf[sub])
            {   // if we run off the end of the subfile, move to the next one.
                if( (offset + length) > (u32)x_flength( pSf[sub] ) )
                {
                    subfileOffset += x_flength( pSf[sub] );
                    offset -= subfileOffset;
                    sub++;
                    ASSERT( offset >= 0 );
                    ASSERT(pSf[sub]);
                }

                // copy the data to the new file.
                ASSERT( offset <= (u32)S32_MAX );
                x_fseek( pSf[sub], (s32)offset, X_SEEK_SET );
                bytesRead = x_fread( pFiledata, 1, length, pSf[sub] );
                ASSERT( bytesRead == length );
            }

            // now write the file
            x_fwrite(pFiledata, 1, pEntry->Length, fp);
            x_fclose(fp);
        }
    }

    // close all the sub files, and clean up.
    for( s32 i=0 ; i < pHeader->nSubFiles ; i++ )
    {
        x_fclose(pSf[i]);
    }

    x_free( pDFS );
    x_free( pFiledata );
    x_free( pSf );
}

//==============================================================================

extern "C" const u16 crc16Table[256];

void dfs_Verify( const xstring& dfsPathName )
{
    // read in the dfs file
    X_FILE* fp = x_fopen(dfsPathName,"rb");
    if( !fp )
        return;

    s32 FSize = x_flength(fp);
    if( FSize < (s32)sizeof(dfs_header) )
    {
        x_fclose(fp);
        return;
    }

    byte* pDFS = (byte*)x_malloc(FSize);
    if(!pDFS)
    {
        x_fclose(fp);
        return;
    }

    x_fread( pDFS, FSize, 1, fp );
    x_fclose(fp);

    dfs_header* pHeader = dfs_InitHeaderFromRawPtr( pDFS );
    if( !pHeader )
    {
        x_free( pDFS );
        return;
    }

    u16 Checksum = pHeader->Checksum;

    const dfs_subfile* pSubFileTable = dfs_GetSubFileTable( pHeader );
    const u16*         pChecksums    = dfs_GetChecksums( pHeader );
    dfs_file*          pEntry        = dfs_GetFiles( pHeader );

    // open the associated sub files
    char DfsSubFile[X_MAX_PATH];
    char DfsDrive[X_MAX_DRIVE];
    char DfsPath[X_MAX_PATH];
    char DfsName[X_MAX_FNAME];
    char DfsExt[X_MAX_EXT];
    x_splitpath((const char*)dfsPathName, DfsDrive, DfsPath, DfsName, DfsExt);

    X_FILE** pSf = (X_FILE**)x_malloc(pHeader->nSubFiles * sizeof(X_FILE*));
    ASSERT( pSf );

    // open all the sub files.
    for( s32 i=0 ; i < pHeader->nSubFiles ; i++ )
    {
        x_sprintf(DfsSubFile, "%s%s%s.%03d", DfsDrive, DfsPath, DfsName, i);

        pSf[i] = x_fopen(DfsSubFile, "rb");
        ASSERT( pSf[i] );
    }

    u32 bufferSize = 32768;
    byte* pFiledata;
    pFiledata = (byte*)x_malloc(bufferSize);
    ASSERT(pFiledata);

    // Get total size of files
    u32 TotalLength = pSubFileTable[pHeader->nSubFiles-1].Offset ;
    u32 Index = 0;
    s32 nSubFile = 0;
    if( (TotalLength % 32768) != 0 )
    {
        x_printf( "Error: data files length not a multiple of 32768\n" );
        goto End;
    }

    while( Index < TotalLength )
    {
        // Find the sub file
        s32 nSubFile = 0;
        u32 SubFileBase = 0;
        while( Index >= pSubFileTable[nSubFile].Offset )
        {
            SubFileBase = pSubFileTable[nSubFile].Offset;
            nSubFile++;
            ASSERT( nSubFile < pHeader->nSubFiles );
        }

        // Read the chunk from the file
        u32 Offset = Index - SubFileBase;
        ASSERT( Offset <= (u32)S32_MAX );
        x_fseek( pSf[nSubFile], (s32)Offset, X_SEEK_SET );
        x_fread( pFiledata, 32768, 1, pSf[nSubFile] );

        // Checksum and compare with stored checksum
        u16 Checksum = 0;
        #define crc16ApplyByte( v, crc ) (u16)((crc << 8) ^  crc16Table[((crc >> 8) ^ (v)) & 255])
        for( s32 i=0 ; i<32768; i++ )
        {
            Checksum = crc16ApplyByte( pFiledata[i], Checksum );
        }
        s32 ChecksumIndex = pSubFileTable[nSubFile].ChecksumIndex + (Offset/32768);
        if( Checksum == pChecksums[ChecksumIndex] )
        {
            x_printf( "." );
        }
        else
        {
            x_printf( "x" );
        }

        Index += 32768;
    }

End:
    // close all the sub files.
    for( s32 i=0 ; i < pHeader->nSubFiles ; i++ )
    {
        x_fclose( pSf[i] );
        pSf[i] = 0;
    }

    x_free( pDFS );
    x_free( pFiledata );
    x_free( pSf );
}
