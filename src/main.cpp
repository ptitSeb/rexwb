#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sox.h>

#include "xwb.h"

uint32_t GetDuration( uint32_t length, const MINIWAVEFORMAT* miniFmt, const uint32_t* seekTable )
{
    switch( miniFmt->wFormatTag )
    {
    case MINIWAVEFORMAT::TAG_ADPCM:
        {
            uint32_t duration = ( length / miniFmt->BlockAlign() ) * miniFmt->AdpcmSamplesPerBlock();
            uint32_t partial = length % miniFmt->BlockAlign();
            if ( partial )
            {
                if ( partial >= ( 7 * miniFmt->nChannels ) )
                    duration += ( partial * 2 / miniFmt->nChannels - 12 );
            }
            return duration;
        }

    case MINIWAVEFORMAT::TAG_WMA:
        if ( seekTable )
        {
            uint32_t seekCount = *seekTable;
            if ( seekCount > 0 )
            {
               return seekTable[ seekCount ] / uint32_t( 2 * miniFmt->nChannels );
            }
        }
        return 0;

    case MINIWAVEFORMAT::TAG_XMA:
        if ( seekTable )
        {
            uint32_t seekCount = *seekTable;
            if ( seekCount > 0 )
            {
               return seekTable[ seekCount ];
            }
        }
        return 0;

    default:
        return ( length * 8 ) / ( miniFmt->BitsPerSample() * miniFmt->nChannels );
    }
}

int main(int argc, const char **argv) {

    int rate = 0;
    int verbose = 1;
    int percentage = 0;
    int force = 0;
    int mono = 0;

    if(argc>3) {
        int t;
        if(sscanf(argv[3], "%d", &t)==1)
            rate=t;
        if (rate<4000) rate=0;
    }
    if(argc>3 && !strcmp(argv[1], argv[2]))
        rate=0;
    if(argc>3) {
        for (int i=4; i<argc; i++) {
            if(!strcmp(argv[i], "-p"))
                {percentage=1; verbose=0;}
            else if(!strcmp(argv[i], "-f"))
                {force=1;}
            else if(!strcmp(argv[i], "-m"))
                {mono=1;}
            else rate = 0;
        }
    }
    if(!rate) {
        printf(
            "usage: %s INFILE.xwb OUTFILE.xwb rate [-f] [-p]\n"
            "Change samplerate to rate of all WaveSound from INFILE to OUTFILE\n"
            "Warning, OUTFILE.xwb is overwiten (and must be different then INFILE.xwb)\n"
            "Use -f to force MS ADPCM to simple PCM\n"
            "Use -m to force mono on all multi-channels ADPCM or PCM sounds\n"
            "Use -p to display percentage (no verbose output, to be used with a zenity progress bar)\n"
            , argv[0]);
        return 1;
    }

    if(sox_init() != SOX_SUCCESS) {
        printf("ERROR: Initializing SOX\n");
        return -3;
    }
    if(!verbose)
        sox_get_globals()->verbosity = 0;

    const char* infile = argv[1];
    const char* outfile = argv[2];

    printf("Will convert %s to %s @%d Hz\n", infile, outfile, rate);

    FILE *fin, *fout;
    fin = fopen(infile, "rb");
    if(!fin) {
        printf("Error opening %s for reading\n", infile);
        return -1;
    }

    WAVEBANKHEADER header;
    WAVEBANKHEADER newheader;
    if ( fread( &header, 1, sizeof(header), fin ) != sizeof(header) )
    {
        printf( "ERROR: File too small for valid wavebank\n");
        fclose(fin);
        return -1;
    }

    if ( *(uint32_t*)header.dwSignature != (uint32_t)WAVEBANK_HEADER_SIGNATURE )
    {
        printf( "ERROR: File is not a wavebank - %ls\n", infile );
        fclose(fin);
        return -1;
    }

    memcpy(&newheader, &header, sizeof(newheader));

    if(verbose)
        printf( "WAVEBANK - %s\n%s\nHeader: File version %u, Tool version %u\n\tBankData %u, length %u\n\tEntryMetadata %u, length %u\n\tSeekTables %u, length %u\n\tEntryNames %u, length %u\n\tEntryWaveData %u, length %u\n",
            infile, "LittleEndian (Windows wave bank)", 
            header.dwHeaderVersion, header.dwVersion,
            header.Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset, header.Segments[WAVEBANK_SEGIDX_BANKDATA].dwLength,
            header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset, header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwLength,
            header.Segments[WAVEBANK_SEGIDX_SEEKTABLES].dwOffset, header.Segments[WAVEBANK_SEGIDX_SEEKTABLES].dwLength,
            header.Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwOffset, header.Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwLength,
            header.Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwOffset, header.Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwLength );

    // check that wavedata are at the end of the file!
    uint32_t waveOffset = header.Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwOffset;
    if(    waveOffset<header.Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwOffset 
        || waveOffset<header.Segments[WAVEBANK_SEGIDX_SEEKTABLES].dwOffset
        || waveOffset<header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset
        || waveOffset<header.Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset
    ) {
        printf("ERROR: Only wxb where WaveData are last entry is supported\n");
        return -3;
    }

    WAVEBANKDATA bank;

    if(fseek(fin, header.Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset, SEEK_SET))
    {
        printf( "ERROR: while fseek on Bankdata\n ");
        fclose(fin);
        return -1;
    }
    if ( fread( &bank, 1, sizeof(bank), fin ) != sizeof(bank) )
    {
        printf( "ERROR: Failed reading bank data\n");
        fclose(fin);
        return -1;
    }

    if(verbose) {
        printf( "Bank Data:\n\tFlags %08X\n", bank.dwFlags );
        printf( "\t\t%s\n", ( bank.dwFlags & WAVEBANK_TYPE_STREAMING ) ? "Streaming" : "In-memory" );
        if ( bank.dwFlags & WAVEBANK_FLAGS_ENTRYNAMES )
        {
            printf( "\t\tFLAGS_ENTRYNAMES\n" );
        }
        if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT )
        {
            printf( "\t\tFLAGS_COMPACT\n" );

            if ( !( bank.dwFlags & WAVEBANK_TYPE_STREAMING ) )
            {
                printf( "WARNING: XACT only supports streaming with compact wavebanks\n");
            }
        }
        if ( bank.dwFlags & WAVEBANK_FLAGS_SYNC_DISABLED )
        {
            printf( "\t\tFLAGS_SYNC_DISABLED\n" );
        }
        if ( bank.dwFlags & WAVEBANK_FLAGS_SEEKTABLES )
        {
            printf( "\t\tFLAGS_SEEKTABLES\n" );
        }
        if ( *bank.szBankName )
            printf( "\tName \"%s\"\n", bank.szBankName );

        printf( "\tEntry metadata size %u\n\tEntry name element size %u\n\tEntry alignment %u\n",
                bank.dwEntryMetaDataElementSize, bank.dwEntryNameElementSize, bank.dwAlignment );
        if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT )
        {
            if ( bank.dwEntryMetaDataElementSize != sizeof(WAVEBANKENTRYCOMPACT) )
            {
                printf( "ERROR: Compact banks expect a metadata element size of %zu\n", sizeof(WAVEBANKENTRYCOMPACT) );
                return 1;
            }
        }
        else
        {
            if ( bank.dwEntryMetaDataElementSize != sizeof(WAVEBANKENTRY) )
            {
                printf( "ERROR: Banks expect a metadata element size of %zu\n", sizeof(WAVEBANKENTRY) );
                return 1;
            }
        }
        if ( ( bank.dwAlignment < WAVEBANK_ALIGNMENT_MIN ) || ( bank.dwAlignment > WAVEBANK_ALIGNMENT_DVD ) )
        {
            printf( "WARNING: XACT expects alignment to be in the range %zu...%zu \n", WAVEBANK_ALIGNMENT_MIN, WAVEBANK_ALIGNMENT_DVD );
        }

        if ( ( bank.dwFlags & WAVEBANK_TYPE_STREAMING ) && ( bank.dwAlignment < WAVEBANK_DVD_SECTOR_SIZE ) )
        {
            printf( "WARNING: XACT expects streaming buffers to be aligned to DVD sector size\n");
        }
    }
/*
    WAVEBANKSYSTEMTIME st;
    if ( FileTimeToSystemTime( &bank.BuildTime, &st ) )
    {
        WAVEBANKSYSTEMTIME lst;
        if ( SystemTimeToTzSpecificLocalTime( nullptr, &st, &lst ) )
        {
            wchar_t szLocalDate[256] = {};
            GetDateFormatW( LOCALE_USER_DEFAULT, DATE_LONGDATE, &lst, nullptr, szLocalDate, 256 );

            wchar_t szLocalTime[256] = {};
            GetTimeFormatW( LOCALE_USER_DEFAULT, 0, &lst, nullptr, szLocalTime, 256 );

            wprintf( L"\tBuild time: %ls, %ls\n", szLocalDate, szLocalTime );
        }
    }
*/
    if ( !bank.dwEntryCount )
    {
        printf( "NOTE: Empty wave bank\n");
        // TODO: Write an empty out file ?
        fout = fopen(outfile, "wb+");
        if(!fout) {
            printf("ERROR: cannot create %s\n", outfile);
            fclose(fin);
            return -2;
        }
        fseek(fout, 0, SEEK_SET);
        fseek(fin, 0, SEEK_END);
        size_t l = ftell(fin);
        fseek(fin, 0, SEEK_SET);
        void* buff = malloc(l);
        fread(buff, 1, l, fin);
        if(fwrite(buff, 1, l, fout)!=l) {
            printf("ERROR: error writing %d bytes\n", l);
            fclose(fout);
            fclose(fin);
            return -2;
        }
        fclose(fout);
        fclose(fin);
        return 0;
    }

    // Entry Names
    uint32_t metadataBytes = header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwLength;
    if ( metadataBytes != ( bank.dwEntryMetaDataElementSize * bank.dwEntryCount ) )
    {
        printf( "ERROR: Mismatch in entries %u and metadata size %u\n", bank.dwEntryCount, metadataBytes );
        fclose(fin);
        return 1;
    }

    if(verbose)
        printf( "%u entries in wave bank:\n", bank.dwEntryCount );

    char* entryNames = NULL;

    uint32_t namesBytes = header.Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwLength;

    if ( namesBytes > 0 )
    {
        if ( namesBytes != ( bank.dwEntryNameElementSize * bank.dwEntryCount ) )
        {
            printf( "ERROR: Mismatch in entries %u and entry names size %u\n", bank.dwEntryCount, namesBytes );
        }
        else
        {
            entryNames = new char[ namesBytes ];

            if ( fseek(fin, header.Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwOffset , SEEK_SET ))
            {
                printf( "ERROR: Failed to seek to entry names data %u\n", header.Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwOffset );
                return 1;
            }

            if ( !fread( entryNames, 1, namesBytes, fin ) != namesBytes )
            {
                printf( "ERROR: Failed reading entry names\n");
                return 1;
            }
        }
    }

    // Seek tables
    uint32_t *seekTables = NULL;

    uint32_t seekLen = header.Segments[WAVEBANK_SEGIDX_SEEKTABLES].dwLength;
    if ( seekLen > 0 )
    {
        if ( seekLen < ( bank.dwEntryCount * sizeof(uint32_t) ) )
        {
            printf( "ERROR: Seek table is too small, needs at least %zu bytes; only %u bytes\n", bank.dwEntryCount * sizeof(uint32_t), seekLen );
        }
        else if ( ( seekLen % 4 ) != 0 )
        {
            printf( "ERROR: Seek table should be a multiple of 4 in size (%u bytes)\n", seekLen );
        }
        else
        {
            size_t seekCount = seekLen / 4;

            seekTables = new uint32_t[ seekCount ];

            if ( fseek( fin, header.Segments[WAVEBANK_SEGIDX_SEEKTABLES].dwOffset, SEEK_SET ))
            {
                printf( "ERROR: Failed to seek to seek tables %u\n", header.Segments[WAVEBANK_SEGIDX_SEEKTABLES].dwOffset );
                return 1;
            }

            if ( fread( seekTables, 1, seekLen, fin )!=seekLen )
            {
                printf( "ERROR: Failed reading seek tables\n");
                return 1;
            }

        }
    }

    if(seekLen>0) {
        // xwb with seekTables not supported for now...
        printf("ERROR: wxb with seekTables are not supported\n");
        return -3;
    }

    // Entries
    uint8_t *entries = NULL;

    if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT )
    {
        entries = reinterpret_cast<uint8_t*>( new WAVEBANKENTRYCOMPACT[ bank.dwEntryCount ] );
    }
    else
    {
        entries = reinterpret_cast<uint8_t*>( new WAVEBANKENTRY[ bank.dwEntryCount ] );
    }

    if ( fseek( fin, header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset, SEEK_SET ) )
    {
        printf( "ERROR: Failed to seek to entry metadata data %u\n", header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset );
        return 1;
    }

    if ( fread( entries, 1, metadataBytes, fin )!=metadataBytes )
    {
        printf( "ERROR: Failed reading entry metadata\n");
        return 1;
    }

    size_t waveBytes = 0;
    size_t newwaveBytes = 0;

    size_t waveLen = header.Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwLength;

    if ( ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT ) && ( waveLen > WAVEBANK_MAX_COMPACT_DATA_SEGMENT_SIZE * bank.dwAlignment ) )
    {
        printf( "ERROR: data segment too large for a valid compact wavebank" );
        return 1;
    }

    // all header analysed, now creating outfile and filing out the hedears...
    fout = fopen(outfile, "wb");
    if(!fout) {
        printf("ERROR: Cannot create %s`\n", outfile);
        fclose(fin);
        return -2;
    }
    // lets bulk copy all headers
    {
        uint32_t t = ftell(fin);
        fseek(fin, 0, SEEK_SET);
        void* buff = malloc(t);
        fread(buff, 1, t, fin);
        if(fwrite(buff, 1, t, fout)!=t) {
            printf("ERROR: Cannot write %d bytes\n", t);
            fclose(fout);
            fclose(fin);
            return -2;
        }
        free(buff);
    }
    // get a copy of entries that will be changed
    uint8_t *newentries = NULL;
    if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT )
    {
        newentries = reinterpret_cast<uint8_t*>( new WAVEBANKENTRYCOMPACT[ bank.dwEntryCount ] );
        memcpy(newentries, entries, sizeof(WAVEBANKENTRYCOMPACT)*bank.dwEntryCount);
    }
    else
    {
        newentries = reinterpret_cast<uint8_t*>( new WAVEBANKENTRY[ bank.dwEntryCount ] );
        memcpy(newentries, entries, sizeof(WAVEBANKENTRY)*bank.dwEntryCount);
    }

    bool hasxma = false;
    for( uint32_t j=0; j < bank.dwEntryCount; ++j)
    {
        uint32_t dwOffset;
        uint32_t dwLength;
        uint32_t Duration = 0;
        uint32_t DurationCompact = 0;
        const MINIWAVEFORMAT* miniFmt;
        MINIWAVEFORMAT* newminiFmt;

        if(percentage)
            printf("%d\n", j*100/bank.dwEntryCount);

        uint32_t* seekTable = nullptr;
        if ( seekTables )
        {
            uint32_t baseOffset = bank.dwEntryCount * sizeof(uint32_t);
            uint32_t offset = seekTables[ j ];
            if ( offset != uint32_t(-1) )
            {
                if ( ( baseOffset + offset ) >= seekLen )
                {
                    printf( "ERROR: Invalid seek table offset entry\n" );
                }
                else
                {
                    seekTable = reinterpret_cast<uint32_t*>( reinterpret_cast<uint8_t*>( seekTables ) + baseOffset + offset );

                    if ( ( ( ( *seekTable + 1 ) * sizeof(uint32_t) ) + baseOffset + offset ) > seekLen )
                    {
                        printf( "ERROR: Too many seek table entries for size of seek tables segment\n");
                    }
                }
            }
        }

        int32_t offset_shift = 0;
        uint32_t dwNewOffset = 0;
        int convert = 1;

        if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT )
        {
            auto& entry = reinterpret_cast<WAVEBANKENTRYCOMPACT*>( entries )[j];

            if(verbose)
                printf( "  Entry %u (%d, %d)\n", j, entry.dwOffset, entry.dwLengthDeviation );

            miniFmt = (const MINIWAVEFORMAT*)&bank.CompactFormat;
            dwOffset = entry.dwOffset * bank.dwAlignment;

            if ( j < ( bank.dwEntryCount - 1 ) )
            {
                dwLength = ( reinterpret_cast<const WAVEBANKENTRYCOMPACT*>( entries )[j + 1].dwOffset * bank.dwAlignment ) - dwOffset - entry.dwLengthDeviation;
            }
            else
            {
                dwLength = static_cast<uint32_t>(waveLen - dwOffset - entry.dwLengthDeviation);
            }

            DurationCompact = GetDuration( dwLength, miniFmt, seekTable );
        }
        else
        {
            auto& entry = reinterpret_cast<WAVEBANKENTRY*>( entries )[j];

            if(verbose) {
                printf( "  Entry %u\n\tFlags %08X\n", j, entry.dwFlags );
                if ( entry.dwFlags & WAVEBANKENTRY_FLAGS_READAHEAD )
                {
                    printf( "\tFLAGS_READAHEAD\n");
                }
                if ( entry.dwFlags & WAVEBANKENTRY_FLAGS_LOOPCACHE )
                {
                    printf( "\tFLAGS_LOOPCACHE\n");
                }
                if ( entry.dwFlags & WAVEBANKENTRY_FLAGS_REMOVELOOPTAIL )
                {
                    printf( "\tFLAGS_REMOVELOOPTAIL\n");
                }
                if ( entry.dwFlags & WAVEBANKENTRY_FLAGS_IGNORELOOP )
                {
                    printf( "\tFLAGS_IGNORELOOP\n");
                }
            }

            miniFmt = &entry.Format;
            dwOffset = entry.PlayRegion.dwOffset;
            dwLength = entry.PlayRegion.dwLength;
            Duration = entry.Duration;
            DurationCompact = GetDuration( entry.PlayRegion.dwLength, miniFmt, seekTable );
        }

        if ( entryNames )
        {
            uint32_t n = bank.dwEntryNameElementSize * j;

            char name[ 64 ] = {};
            strncpy( name, &entryNames[ n ], 64 );
            if(verbose)
                printf( "\t\"%s\"\n", name );
        }


        if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT )
        {
            float seconds = float( DurationCompact ) / float( miniFmt->nSamplesPerSec );
            if(verbose)
                printf( "\tEstDuration %u samples (%f seconds)\n", DurationCompact, seconds );
        }
        else
        {
            float seconds = float( Duration ) / float( miniFmt->nSamplesPerSec );
            if(verbose)
                printf( "\tDuration %u samples (%f seconds), EstDuration %u\n", Duration, seconds, DurationCompact );
        }

        if(verbose)
            printf( "\tPlay Region %u, Length %u\n", dwOffset, dwLength );

        if ( !( bank.dwFlags & WAVEBANK_FLAGS_COMPACT ) )
        {
            auto& entry = reinterpret_cast<const WAVEBANKENTRY*>( entries )[j];

            if(verbose)
                if ( entry.LoopRegion.dwTotalSamples > 0 )
                {
                    printf( "\tLoop Region %u...%u\n", entry.LoopRegion.dwStartSample, entry.LoopRegion.dwTotalSamples );
                }
        } else convert = 0; // no conversion for compact format for now

        const char* fmtstr = nullptr;
        switch( miniFmt->wFormatTag )
        {
        case MINIWAVEFORMAT::TAG_PCM:   fmtstr = "PCM"; break; 
        case MINIWAVEFORMAT::TAG_ADPCM: fmtstr = "MS ADPCM"; break;
        case MINIWAVEFORMAT::TAG_WMA:   fmtstr = "xWMA"; convert=0; break;
        case MINIWAVEFORMAT::TAG_XMA:   fmtstr = "XMA"; convert=0; break;
        }

        if(verbose)
            printf( "\t%s %u channels, %u-bit, %u Hz\n\tblockAlign %u, avgBytesPerSec %u\n",
                fmtstr,
                miniFmt->nChannels, miniFmt->BitsPerSample(), miniFmt->nSamplesPerSec,
                miniFmt->BlockAlign(), miniFmt->AvgBytesPerSec() );

        if ( !dwLength )
        {
            printf( "ERROR: Entry length is 0\n");
        }

        if ( dwOffset > waveLen
             || (dwOffset+dwLength) > waveLen )
        {
            printf( "ERROR: Invalid wave data region for entry\n");
        }

        if ( ( dwOffset % bank.dwAlignment ) != 0 )
        {
            printf( "ERROR: Entry offset doesn't match alignment\n");
        }

        if ( seekTable )
        {
            if(verbose)
                printf( "\tSeek table with %u entries", *seekTable );

            for ( uint32_t k = 0; k < *seekTable; ++k )
            {
                if ( verbose && ( k % 6 ) == 0 )
                    printf( "\n\t");

                if(verbose)
                    printf( "%u ", seekTable[ k + 1 ] );
            }

            if(verbose)
                printf( "\n");
        }

        switch( miniFmt->wFormatTag  )
        {
        case MINIWAVEFORMAT::TAG_XMA:
            hasxma = true;
            if ( ( dwOffset % 2048 ) != 0 )
            {
                printf( "ERROR: XMA2 data needs to be aligned to a 2K boundary\n" );
            }

            if ( !seekTable )
            {
                printf( "ERROR: Missing seek table entry for XMA2 wave\n" );
            }
            break;

        case MINIWAVEFORMAT::TAG_WMA:
            if ( !seekTable )
            {
                printf( "ERROR: Missing seek table entry for xWMA wave\n" );
            }
            break;
        }

        int adpcm_in = miniFmt->wFormatTag==MINIWAVEFORMAT::TAG_ADPCM?1:0;
        int adpcm_out = (force)?0:adpcm_in;

        // convert!
        {
            // load the data in a buffer
            void* buffin = malloc(dwLength + (convert?(adpcm_in?sizeof(WAVHEADER_ADPCM):sizeof(WAVHEADER_SIMPLE)):0));
            fseek(fin, waveOffset + dwOffset, SEEK_SET);
            char* p = (char*)buffin;
            if(convert) {
                if(adpcm_in) {
                    // Write MS ADPCM WAV Header
                    WAVHEADER_ADPCM head;
                    memcpy(&head.sign, "RIFF", 4);
                    head.filesize = dwLength + sizeof(WAVHEADER_ADPCM) - 8;
                    memcpy(&head.format, "WAVE", 4);
                    // fmt
                    memcpy(&head.formatid, "fmt ", 4);
                    head.blocksize = 0x10 + 32 + 2;
                    head.audioformat = 2;
                    head.channels = miniFmt->nChannels;
                    head.rate = miniFmt->nSamplesPerSec;
                    head.bytepersec = miniFmt->AvgBytesPerSec();
                    head.byteperblock = miniFmt->BlockAlign();
                    head.bitspersample = miniFmt->BitsPerSample();
                    // extra
                    head.extrassz = 32;
                    head.newsz =  (((head.byteperblock - (7 * head.channels)) * 8) / (head.bitspersample * head.channels)) + 2;
                    head.ncoeff = 7;
                    head.coef[0] = 0x00000100;
                    head.coef[1] = 0xFF000200;
                    head.coef[2] = 0x00000000;
                    head.coef[3] = 0x004000C0;
                    head.coef[4] = 0x000000F0;
                    head.coef[5] = 0xFF3001CC;
                    head.coef[6] = 0xFF180188;
                    memcpy(&head.factid, "fact", 4);
                    head.factsz = 4;
                    if(head.byteperblock && head.channels) {
                        head.factdata = ((head.byteperblock - (7 * head.channels)) * 8) / head.bitspersample;
                        head.factdata = (dwLength / head.byteperblock ) * head.factdata;
                        head.factdata /= head.channels;
                    } else
                        head.factdata = 0;
                    // data
                    memcpy(&head.blockid, "data", 4);
                    head.datasize = dwLength;
                    memcpy(p, &head, sizeof(head));
                    p+=sizeof(head);  // WAV Header
                } else {
                    // Write simple PCM WAV Header
                    WAVHEADER_SIMPLE head;
                    memcpy(&head.sign, "RIFF", 4);
                    head.filesize = dwLength + 44 - 8;
                    memcpy(&head.format, "WAVE", 4);
                    // fmt
                    memcpy(&head.formatid, "fmt ", 4);
                    head.blocksize = 0x10;
                    head.audioformat = 1;
                    head.channels = miniFmt->nChannels;
                    head.rate = miniFmt->nSamplesPerSec;
                    head.bytepersec = miniFmt->AvgBytesPerSec();
                    head.byteperblock = miniFmt->BlockAlign();
                    head.bitspersample = miniFmt->BitsPerSample();
                    // data
                    memcpy(&head.blockid, "data", 4);
                    head.datasize = dwLength;
                    memcpy(p, &head, sizeof(head));
                    p+=sizeof(head);  // WAV Header
                }
            }
            if(fread(p, 1, dwLength, fin)!=dwLength) {
                printf("ERROR: reading wav data!\n");
                return -1;
            }
            int newLength, newDuration, newrate, newsamples, newBlockAlign, newchannels;
            void* buffout = NULL;
            if (convert) {
                // find the new rate
                newchannels = mono?1:miniFmt->nChannels;
                uint32_t nsamples = Duration * newchannels;
                int nblocks = dwLength / miniFmt->BlockAlign();
                nblocks = (uint64_t)nblocks * rate / miniFmt->nSamplesPerSec;
                newLength = nblocks * miniFmt->BlockAlign();
                if(adpcm_in != adpcm_out)   // converting adpcm -> PCM : size * 4!
                    newLength *= 4;
                newDuration = (uint64_t)Duration * rate / miniFmt->nSamplesPerSec;
                newrate = (uint64_t)newDuration * miniFmt->nSamplesPerSec / Duration;
                newsamples = newDuration * newchannels;
                newBlockAlign = miniFmt->wBlockAlign;
                // Using Mem Buffer as input seems to have some nasty side effects in the long run... So using an actual file instead for now.
                //sox_format_t * format_in = sox_open_mem_read(buffin, dwLength + (adpcm_in?sizeof(WAVHEADER_ADPCM):sizeof(WAVHEADER_SIMPLE)), NULL, NULL, "WAV");
                #define TMPWAV "/tmp/rewxb_tmp.wav"
                {
                    FILE *tmp = fopen(TMPWAV, "wb");
                    fwrite(buffin, 1, dwLength + (adpcm_in?sizeof(WAVHEADER_ADPCM):sizeof(WAVHEADER_SIMPLE)), tmp);
                    fclose(tmp);
                }
                sox_format_t * format_in = sox_open_read(TMPWAV, NULL, NULL, "WAV");
                if(!format_in) {
                    printf("ERROR: SOX cannot create read format\n");
                    return -3;
                }
                // copy in to out format
                sox_signalinfo_t signal_out = {};
                sox_encodinginfo_t encoding_out;
                signal_out.channels = newchannels;
                signal_out.rate = newrate;
                signal_out.length = newLength*4; // some margin? Cannot use SOX_UNKNOWN_LEN here, maybe because format_in is a mem buffer and so non-seekable?
                signal_out.precision = format_in->signal.precision;
                memcpy(&encoding_out, &format_in->encoding, sizeof(encoding_out));
                if(adpcm_in != adpcm_out) {   // converting adpcm -> PCM
                    encoding_out.encoding = SOX_ENCODING_SIGN2;
                    encoding_out.bits_per_sample = 16;
                    signal_out.precision = 16;
                }
                sox_format_t * format_out = NULL;
                size_t buffer_size;
                format_out = sox_open_memstream_write((char**)&buffout, &buffer_size, &signal_out, &encoding_out, "WAV", NULL);
                if(!format_out) {
                    printf("ERROR: SOX cannot create write format\n");
                    return -3;
                }

                sox_signalinfo_t interm_signal = format_in->signal;
                sox_effects_chain_t *chain = sox_create_effects_chain(&format_in->encoding, &format_out->encoding);
                char * args[10];
                sox_effect_t *e = sox_create_effect(sox_find_effect("input"));
                args[0] = (char *)format_in;
                if(sox_effect_options(e, 1, args) != SOX_SUCCESS) {
                    printf("ERROR: SOX cannot validate input effect\n");
                    return -3;
                }
                if(sox_add_effect(chain, e, &interm_signal, &format_in->signal) != SOX_SUCCESS) {
                    printf("ERROR: SOX cannot add input effect\n");
                    return -3;
                }
                free(e);
                e = sox_create_effect(sox_find_effect("rate"));
                if(sox_effect_options(e, 0, NULL) != SOX_SUCCESS) {
                    printf("ERROR: SOX cannot validate rate effect\n");
                    return -3;
                }
                if(sox_add_effect(chain, e, &interm_signal, &format_out->signal) != SOX_SUCCESS) {
                    printf("ERROR: SOX cannot add rate effect\n");
                    return -3;
                }
                free(e);
                if(format_in->signal.channels != format_out->signal.channels) {
                    e = sox_create_effect(sox_find_effect("channels"));
                    if(sox_effect_options(e, 0, NULL) != SOX_SUCCESS) {
                        printf("ERROR: SOX cannot validate rate effect\n");
                        return -3;
                    }
                    if(sox_add_effect(chain, e, &interm_signal, &format_out->signal) != SOX_SUCCESS) {
                        printf("ERROR: SOX cannot add rate effect\n");
                        return -3;
                    }
                    free(e);
                }
                e = sox_create_effect(sox_find_effect("output"));
                args[0] = (char *)format_out;
                if(sox_effect_options(e, 1, args) != SOX_SUCCESS) {
                    printf("ERROR: SOX cannot validate out effect\n");
                    return -3;
                }
                if(sox_add_effect(chain, e, &interm_signal, &format_out->signal) != SOX_SUCCESS) {
                    printf("ERROR: SOX cannot add out effect\n");
                    return -3;
                }
                free(e);
                // convert !
                if(verbose)
                    printf("\tConvert %u/%u:%dHz -> %u/%u:%dHz\n", dwLength, Duration, miniFmt->nSamplesPerSec, newLength, newDuration, newrate);

                int err = sox_flow_effects(chain, NULL, NULL);
                if(err!=SOX_SUCCESS) {
                    printf("ERROR: SOX: %s\n", sox_strerror(err));
                }

                sox_delete_effects_chain(chain);
                sox_close(format_out);
                sox_close(format_in);
                // read back the file (only for adpcm, for PCM it's already in the memory buffer as RAW)
                if(adpcm_out) {
                    WAVHEADER_ADPCM *head = (WAVHEADER_ADPCM*)buffout;
                    // check the header is as expected
                    if(memcmp(&head->sign, "RIFF", 4)) {
                        printf("ERROR: Converted WAV is not a WAV file???\n");
                        return -3;
                    }
                    if(head->extrassz!=32 || memcmp(&head->factid, "fact", 4) || head->factsz!=4) {
                        printf("ERROR: Converted WAV doesn't have the expected header...\n");
                        return -3;
                    }
                    newLength = buffer_size - sizeof(WAVHEADER_ADPCM);//head->datasize;
                    newrate = head->rate;
                    newchannels = head->channels;
                    newBlockAlign = head->byteperblock/head->channels - MINIWAVEFORMAT::ADPCM_BLOCKALIGN_CONVERSION_OFFSET;
                    newDuration = newLength * 8 / (newchannels*4);
                    p = (char*)buffout+sizeof(WAVHEADER_ADPCM);
                } else {
                    WAVHEADER_SIMPLE *head = (WAVHEADER_SIMPLE*)buffout;
                    // check the header is as expected
                    if(memcmp(&head->sign, "RIFF", 4)) {
                        printf("ERROR: Converted WAV is not a WAV file???\n");
                        return -3;
                    }
                    if(head->blocksize!=16) {
                        printf("ERROR: Converted WAV doesn't have the expected header...(0x%x!=0x10)\n", head->blocksize);
                        return -3;
                    }
                    newLength = buffer_size - sizeof(WAVHEADER_SIMPLE);//head->datasize;
                    newrate = head->rate;
                    newchannels = head->channels;
                    newBlockAlign = head->byteperblock;
                    newDuration = newLength * 8 / (newchannels*16); 
                    p = (char*)buffout+sizeof(WAVHEADER_SIMPLE);
                }
                remove(TMPWAV);
            } else {
                newLength = dwLength;
                buffout = buffin;
                p = (char*)buffout;
            }
            if(!newLength) {
                printf("ERROR: null buffer!\n");
                return -5;
            }
            // save data to file (check offset?)
            uint32_t newOffset = ftell(fout);
            fwrite(p, 1, newLength, fout);
            newwaveBytes += newLength;
            if(convert) {
                auto& newentry = reinterpret_cast<WAVEBANKENTRY*>( newentries )[j];
                newminiFmt = &newentry.Format;
                if(verbose)
                    printf("\tnew entry %u->%u/%dx%dHz %s -> ", newentry.PlayRegion.dwOffset, newentry.PlayRegion.dwLength, newminiFmt->nChannels, newminiFmt->nSamplesPerSec, adpcm_in?"MS_ADPCM":"PCM");
                newentry.PlayRegion.dwOffset = newOffset - waveOffset;
                newentry.PlayRegion.dwLength = newLength;
                newentry.Duration = newDuration;
                uint64_t oldrate = newminiFmt->nSamplesPerSec;
                newminiFmt->nSamplesPerSec = newrate;
                newminiFmt->wBlockAlign = newBlockAlign;
                newminiFmt->nChannels = newchannels;
                if(adpcm_in != adpcm_out) {
                    newminiFmt->wFormatTag=MINIWAVEFORMAT::TAG_PCM;
                    newminiFmt->wBitsPerSample=1; // 16bits
                }
                if(verbose)
                    printf("%u->%u/%dx%dHz %s\n", newentry.PlayRegion.dwOffset, newentry.PlayRegion.dwLength, newminiFmt->nChannels, newminiFmt->nSamplesPerSec, adpcm_out?"MS_ADPCM":"PCM");
                if ( newentry.LoopRegion.dwTotalSamples > 0 )
                {
                    newentry.LoopRegion.dwStartSample = ((uint64_t)(newentry.LoopRegion.dwStartSample/32) * newrate / oldrate)*32;
                    newentry.LoopRegion.dwTotalSamples = ((uint64_t)(newentry.LoopRegion.dwTotalSamples/32) * newrate / oldrate)*32;
                    if(newentry.LoopRegion.dwTotalSamples>newDuration)
                        newentry.LoopRegion.dwTotalSamples=newDuration;
                }
            } else {
                if ( bank.dwFlags & WAVEBANK_FLAGS_COMPACT ) {
                    auto& newentry = reinterpret_cast<WAVEBANKENTRYCOMPACT*>( newentries )[j];
                    newentry.dwOffset = dwOffset / bank.dwAlignment;
                } else {
                    auto& newentry = reinterpret_cast<WAVEBANKENTRY*>( newentries )[j];
                    newentry.PlayRegion.dwOffset = newOffset - waveOffset;
                }
            }
            // add some padding if lenght is not aligned
            if(newLength%bank.dwAlignment) {
                uint8_t pad[2048] = {};
                fwrite(pad, 1, bank.dwAlignment-(newLength%bank.dwAlignment), fout);
                newwaveBytes += bank.dwAlignment-(newLength%bank.dwAlignment);
            }
            if(buffout!=buffin)
                free(buffout);
            free(buffin);
        }

        waveBytes += dwLength;
    }

    sox_quit();

    if ( hasxma )
    {
        if ( ( header.Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwOffset % 2048 ) != 0 )
        {
            printf( "WARNING: Wave banks containing XMA2 data should have the wave segment offset aligned to a 2K boundary\n" );
        }
    }

    if(verbose)
        printf( "  Total wave bytes %zu -> %zu\n", waveBytes, newwaveBytes );

    if ( waveBytes > waveLen )
    {
        printf( "ERROR: Invalid wave data region\n");
    }

    // write back new entries
    uint32_t t = ftell(fout);
    if ( fseek( fout, header.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset, SEEK_SET ) )
    {
        printf( "ERROR: Failed to seek to entry metadata data %u on out file\n", waveOffset );
        return -2;
    }

    if ( fwrite( newentries, 1, metadataBytes, fout )!=metadataBytes ) {
        printf(" ERROR: Failed to write updated entrie table\n");
        return -2;
    }
    // write back header with new wavesize
    newheader.Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwLength = newwaveBytes;
    fseek( fout, 0, SEEK_SET );
    fwrite( &newheader, 1, sizeof(newheader), fout);

    // go to the end and truncate (incase the file already exist)
    fseek(fout, t, SEEK_SET);
    fclose(fout);

    delete[] entryNames;
    delete[] seekTables;
    delete[] entries;

    fclose(fin);

    return 0;
}