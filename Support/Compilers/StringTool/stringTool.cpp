//==============================================================================
//
//  StringTool.cpp
//
//  String table conversion tool
//
//==============================================================================

#include "x_files.hpp"
#include "x_bytestream.hpp"
#include "Auxiliary/CommandLine/CommandLine.hpp"
#include "Parsing/textout.hpp"
#include "../../StringMgr/StringMgr.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define MAX_COLUMNS     5  // Number of columns we are interested in
#define MAX_PLATFORMS   1
#define PLATFORM_PC     0

//==============================================================================
//  DISPLAY HELP
//==============================================================================

void DisplayHelp( void )
{
    x_printf( "\n" );
    x_printf( "StringTool (c)2001-2025 Inevitable Entertainment Inc.\n" );
    x_printf( "\n" );
    x_printf( "KSS: PS2,XBOX and GCN is not supported. Please use older version.\n" );
    x_printf( "\n" );	
    x_printf( "  usage:\n" );
    x_printf( "         stringTool [-opt [param]] [txtfile|binfile]\n" );
    x_printf( "\n" );
    x_printf( "options:\n" );
    x_printf( "         -debug            - Output DEBUG ID names (col 1)\n" );
    x_printf( "         -pc               - Output destination for PC platform\n" );


    x_printf( "File Structure\n" );
    x_printf( "Signature(32)                //'STRB'\n" );
    x_printf( "Version(32)                  //Format version\n" );
    x_printf( "nStrings(32)                 //Number of strings\n" );
    x_printf( "StringOffsets[n](32)         //Offsets to String IDs\n" );
    x_printf( "StringID(null)String(null)   //String ID followed by Text String\n" );
    x_printf( "...                          //\n" );
    x_printf( "StringID(null)String(null)   //Repeated nStrings times\n" );
    x_printf( "\n" );
}


//==============================================================================
//  HEX VALUE
//==============================================================================

s32 HexValue( xwchar H )
{
    s32 Value;
    if( IN_RANGE( '0', H, '9' ) )   Value = H - '0';
    if( IN_RANGE( 'A', H, 'F' ) )   Value = H - 'A' + 10;
    if( IN_RANGE( 'a', H, 'f' ) )   Value = H - 'a' + 10;
    return( Value );
}

//==============================================================================
//  MAIN
//==============================================================================

int main( int argc, char** argv )
{
    command_line    CommandLine;
    xbool           NeedHelp;
    s32             iOpt;
    xstring         OutputFolder;
    xstring         Prefix;
    xbool           DoPrefix        = FALSE;
    xbool           Overwrite       = TRUE;
	xbool			Info			= FALSE;
	xbool			SubTitleMode    = TRUE;
    xbool           DebugOutput     = FALSE;
    xstring         BinName[ MAX_PLATFORMS ];
    s32             i;
    s32             iDefine = 0;
    s32             NextColOffset = 0;

    //-- Init x_Files and Memory
    x_Init( argc, argv );

    // Setup recognized command line options
	CommandLine.AddOptionDef( "DEBUG" );
    CommandLine.AddOptionDef( "PC", command_line::STRING );

    // Parse command line
    NeedHelp = CommandLine.Parse( argc, argv );
    
    if( NeedHelp || (CommandLine.GetNumArguments() == 0) )
    {
        DisplayHelp();
        return 10;
    }

    // Check output folder option
    iOpt = CommandLine.FindOption( xstring("PC") );
    if( iOpt != -1 )
    {
        BinName[PLATFORM_PC] = CommandLine.GetOptionString( iOpt );
    }
    else
    {
        BinName[PLATFORM_PC].Clear();
    }

    DebugOutput = (CommandLine.FindOption( xstring("DEBUG") ) != -1 );

    x_printf(xfs("\nStringTool started on (%s) file.\n",CommandLine.GetArgument( 0 )));

    // Loop through all the files
    for( i=0 ; i<CommandLine.GetNumArguments() ; i++ )
    {
        // Get Pathname of file
        const xstring& TextName = CommandLine.GetArgument( i );

        //     = CommandLine.ChangeExtension( TextName, "stringbin" );
        xstring     CodeName	= CommandLine.ChangeExtension( TextName, "cpp" );
        xstring     OutName	    = CommandLine.ChangeExtension( TextName, "info" );
		text_out	tOut;

        xstring     Path;
        xstring     File;
        xwstring    Text;
        xarray<xwstring>    SortedStrings;
        xarray<xstring>     StringsID;
        xwstring    Line;
        s32         Index       = 0;
        s32         nDefine     = 0;
        s32         iIndex;
        s32         nEntries    = 0;
        s32         nColumns;
        s32         Column[MAX_COLUMNS];
        const xwchar term = '\0';

        xbytestream Binary;
        xbytestream IndexTable;

		if( Info )
		{
			x_try;
            
                tOut.OpenFile( OutName );
            
            x_catch_begin;

                x_printf( "Error Opening Info FILE\n" );
				return 0;

            x_catch_end;
		}


        // Load the string
        if( Text.LoadFile( TextName ) == FALSE )
            x_printf( "Error Loading File - \"%s\" \n", TextName );

		StringsID.Clear();

        // Skip blank lines at beginning
        while( (Index < Text.GetLength()) && ((Text[Index] == 0x0d) || (Text[Index] == 0x0a)) ) 
            Index++;

        // Binary sort the strings.
        SortedStrings.Clear();
        xbool IsInserted = FALSE;
        s32 i;
        xbool IDFound = FALSE;
        while( (Index < Text.GetLength()) && (Text[Index] != 0x00) )
        {
            if( Text[Index] == 0x09 )
                IDFound = TRUE;

            if( (Text[Index] == 0x0d) )
            {
                Line += Text[Index];
                Index++;
                
                if( Text[Index] == 0x0a )
                {
                    Line += Text[Index];
                    Index++;
                }

                //Line += term;

                for( i = 0 ; i < SortedStrings.GetCount() ; i++ )
                {

                    s32 retval = x_wstrcmp( Line, SortedStrings[i] );

                    if( retval < 0 )
                    {
                        SortedStrings.Insert( i, Line );
                        IsInserted = TRUE;
                        break;
                    }
                }

                if( !IsInserted )
                    SortedStrings.Append( Line );
               
                IsInserted = FALSE;
                IDFound = FALSE;
                Line.Clear();
            }
            else
            {
                if( !IDFound )
                    Line += x_toupper( (const char)Text[Index] );
                else
                    Line += Text[Index];

                Index++;
            }
        }

        Text.Clear();
        for( i = 0 ; i < SortedStrings.GetCount() ; i ++ )
            Text += SortedStrings[i];

        // Skip blank lines at beginning
        Index = 0;
        while( (Index < Text.GetLength()) && ((Text[Index] == 0x0d) || (Text[Index] == 0x0a)) ) 
            Index++;

        // Compile to Binary and Header
        do
        {
            // Read line from Text File
            nColumns  = 0;
            Column[0] = Index;
            while( (Index < Text.GetLength()) && (Text[Index] != 0x00) )
            {
                // Check for new column
                if( Text[Index] == 0x09 )
                {
                    Text[Index] = 0;
                    Index++;
                    nColumns++;
                    if( nColumns < MAX_COLUMNS )
                        Column[nColumns] = Index;
                }

                // Check for end of line
                else if( (Text[Index] == 0x0d) )
                {
                    Text[Index] = 0;
                    Index++;
                    if( Text[Index] == 0x0a )
                    {
                        Text[Index] = 0;
                        Index++;
                    }
                    nColumns++;
                    if( nColumns < MAX_COLUMNS )
                        Column[nColumns] = Index;

                    // Exit enclosing while
                    break;
                }

                // Just advance to next character
                else
                    Index++;
            }

            // Is it a valid line?
            NextColOffset=0;

            if( (nColumns > 0) && (x_wstrlen( &Text[Column[0]] ) > 0) /*&& (x_wstrlen( &Text[Column[1]] ) > 0)*/ )
            {
                if( DebugOutput )
                {
                    x_printf("%s\n",xfs("<%04i> %s",nEntries ,(const char*)xstring(&Text[Column[0]])));
                    x_printf("%s\n",xfs("<p>%s", (const char*)xstring(&Text[Column[1]])));
                    x_printf("%s\n",xfs("<p>%s", (const char*)xstring(&Text[Column[2]])));
                }

                // Process text to remove bogus characters that excel kindly places in there
                {
                    s32 End = Column[1] + x_wstrlen( &Text[Column[1]] );
                        
                    for( s32 i=Column[1]; i<End; i++ )
                    {
                        // Extra hack to handle playstation 2 action cluster buttons
                        if( Text[i] == 0x2013 ) Text[i] = '!';
                        if( Text[i] == 0x2018 ) Text[i] = '\'';
                        if( Text[i] == 0x2019 ) Text[i] = '\'';
                        if( Text[i] == 0x201C ) Text[i] = '\"';
                        if( Text[i] == 0x201D ) Text[i] = '\"';
                        if( Text[i] == 0x2122 ) Text[i] = 0x0012;   // tm
                        if( Text[i] == 0x201E ) Text[i] = 0x0011;   // German leading Quote
                        if( Text[i] == 0x2026 || Text[i] == 0x0085)
                        {
                            Text[i] = '.';
                            Text.Insert( i, xwstring(".") );
                            Text.Insert( i, xwstring(".") );
                            End += 2;
                            NextColOffset+=2;
                        }

                        if( Text[i] == 0x0092 )
                            Text[i] = '\'';
                    }
                }


                //-- IDS for Info File.
				StringsID.Append( xstring( xfs("%s", (const char*)xstring(&Text[Column[0]]))) );

                nDefine++;

                // Write Offset into index table
                iIndex = Binary.GetLength();
                if (DebugOutput)
                    x_printf("index = %d\n", iIndex);

                IndexTable += (byte)((iIndex>> 0)&0xff);
                IndexTable += (byte)((iIndex>> 8)&0xff);
                IndexTable += (byte)((iIndex>>16)&0xff);
                IndexTable += (byte)((iIndex>>24)&0xff);

                // Write wide string into Binary
                if( nColumns > 1 )
                {
                    s32     Length = x_wstrlen( &Text[Column[1]] );
                    xwchar* pSearch;

                    // Adjust string to remove enclosing quotes (automatically placed by excel around
                    // lines that have an embedded newline)
                    if( Length > 1 )
                    {
                        if( (Text[Column[1]] == '"') && (Text[Column[1]+Length-1] == '"') )
                        {
                            Column[1]++;
                            Length -= 2;
                       }
                    }

                    xwchar* pQuote;
                    pSearch = &Text[Column[1]];

                    while( (pQuote = x_wstrstr( pSearch, xwstring( "\"\"" ) )) )
                    {
                        s32 Position = pQuote - (const xwchar*)Text;
                        Text.Delete( Position );
                        Index  -= 1;
                        Length -= 1;
                        pSearch = pQuote + 1;
                    }

                    // Look for embedded color codes.
                    // Form is "~RRGGBB~" with the RGB values in hex.
                    // Output is 0xFFRR, 0xGGBB where RGB are non-zero byte values.

                    xwchar* pColor;
                    pSearch = &Text[Column[1]];

                    while( (pColor = x_wstrchr( pSearch, '~' )) )
                    {
                        s32 Position = pColor - (const xwchar*)Text;

                        if( Text[Position+7] != '~' )
                        {
                            pSearch = pColor + 1;
                            continue;
                        }

                        xwchar  P = 0;
                        xwchar  Q = 0;
                        u8      R, G, B;

                        R   = HexValue( Text[Position+1] );
                        R <<= 4;
                        R  += HexValue( Text[Position+2] );

                        G   = HexValue( Text[Position+3] );
                        G <<= 4;
                        G  += HexValue( Text[Position+4] );

                        B   = HexValue( Text[Position+5] );
                        B <<= 4;
                        B  += HexValue( Text[Position+6] );

                        P   = 0xFF;
                        P <<= 8;
                        P  |= R;

                        Q   = G;
                        Q <<= 8;
                        Q  |= B;

                        if( Q == 0 )    Q = 1;  // Don't let a color make a NULL.

                        Text.Delete( Position+1, 6 );
                        Text[Position+0] = P;
                        Text[Position+1] = Q;

                        Length -= 6;
                        Index  -= 6;

                        pSearch = pColor + 1;
                    }

                    
                    //-- Append Title
                    Binary.Append( (byte*)&Text[Column[0]], x_wstrlen( &Text[Column[0]] )*2 );
                    Binary.Append((byte*)&term,sizeof(xwchar));

                    //-- Append String
                    Binary.Append( (byte*)&Text[Column[1]], Length*2 );
                    Binary.Append((byte*)&term,sizeof(xwchar));

                    //-- If SubtitleMode then output the name of the speaker from Column 3
                    if( SubTitleMode && nColumns > 2)
                    {
                        if( x_wstrlen( &Text[Column[2]] ) > 0 )
                        {
                            Binary.Append( (byte*)&Text[Column[2]+NextColOffset], (x_wstrlen( &Text[Column[2]+NextColOffset] )*2)  );
                            Binary.Append((byte*)&term,sizeof(xwchar));
                        }
                        else
                            Binary.Append((byte*)&term,sizeof(xwchar));
                    }
                    else if( SubTitleMode )
                        Binary.Append((byte*)&term,sizeof(xwchar));

                    if( nColumns > 3 )
                    {
                        if( x_wstrlen( &Text[Column[3]] ) > 0 )
                        {
                            Binary.Append( (byte*)&Text[Column[3]+NextColOffset], (x_wstrlen( &Text[Column[3]+NextColOffset] )*2)  );
                            Binary.Append((byte*)&term,sizeof(xwchar));
                        }
                        else
                        {
                            Binary.Append( (byte*)&term, sizeof(xwchar));
                        }
                    }
                    else
                    {
                        Binary.Append( (byte*)&term, sizeof(xwchar));
                    }
                }
                else
                {
                    Binary.Append((byte*)&term,sizeof(xwchar));
                }
                // Increment number of entries
                nEntries++;
            }
            // Advance to beginning of next line
            while( (Index < Text.GetLength()) && 
                   ((Text[Index] == 0x00) || 
                    (Text[Index] == 0x0d) || 
                    (Text[Index] == 0x0a)) ) 
                Index++;

        } while( Index < Text.GetLength() );

		//-- Info Output does not need a header.. .info file is a loaded header file so we dont have to recompile the game to add new
		//-- lines of text to the Excel strings file.
		if( Info )
		{
	        x_printf( "Saveing  \"%s\"\n", OutName );			
			tOut.AddHeader( "Strings", StringsID.GetCount());
			for( s32 index = 0 ; index < StringsID.GetCount(); index++ )
			{
				tOut.AddString( "StringID",	(const char *)StringsID[index] );
				tOut.AddEndLine();
			}
			tOut.CloseFile();
		}

        // Write Binary
        stringbin_header FileHeader;
        x_memset( &FileHeader, 0, sizeof( FileHeader ) );
        FileHeader.Signature   = STRINGBIN_SIGNATURE;
        FileHeader.Version     = STRINGBIN_VERSION;
        FileHeader.StringCount = nEntries;

        xbytestream Header;
        Header.Append( (const byte*)&FileHeader, sizeof( FileHeader ) );
        Header.Append( IndexTable );

                const s32 HeaderLength = Header.GetLength();
                Binary.Insert( 0, Header );

	    for( s32 iPlatform = PLATFORM_PC; iPlatform < MAX_PLATFORMS; iPlatform++ )
        {	
            if( BinName[iPlatform].GetLength() == 0 )
                continue;

            if( Overwrite || !CommandLine.FileExists( BinName[iPlatform] ) )
		    {
			    // Save the file
			    if( !Binary.SaveFile( BinName[iPlatform] ) )
			    {
				    x_printf( "Error - Saving Binary \"%s\"\n", BinName[iPlatform] );
			    }
			    else
			    {
				    x_printf( "Saving Binary \"%s\"\n", BinName[iPlatform] );
			    }
		    }
		    else
		    {
			    // Display error
			    x_printf( "Error - File \"%s\" already exists\n", BinName[iPlatform] );
		    }
        }
    } 

    // Return success
    x_Kill();
    return 0;
}