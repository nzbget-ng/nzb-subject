
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "yxml.h"

// #define DEBUG_VERBOSE 1

typedef enum {
    kToken_Separator = 1,
    kToken_Unquoted, // 2
    kToken_Quoted,   // 3
    kToken_Empty,    // 4
    kToken_String,   // 5
    kToken_Number,   // 6
    kToken_WtFnZb,   // 7
    kToken_PRiVATE,  // 8
    kToken_PRiV_WtF, // 9
    kToken_N3wZ,     // a
    kToken_newzNZB,  // b
    kToken_FULL,     // c
    kToken_yEnc,     // d
    kTokenTypeMax    // e
} tTokenType;


static const char * tokenTypeNames[ kTokenTypeMax ] = {
    [kToken_Separator] = "separator",
    [kToken_Unquoted]  = "unquoted",
    [kToken_Empty]     = "empty",
    [kToken_String]    = "string",
    [kToken_Number]    = "number",
    [kToken_Quoted]    = "quoted",
    [kToken_WtFnZb]    = "WtFnZb",
    [kToken_PRiVATE]   = "PRiVATE",
    [kToken_PRiV_WtF]  = "PRiV-WtF",
    [kToken_N3wZ]      = "N3wZ",
    [kToken_newzNZB]   = "newzNZB",
    [kToken_FULL]      = "FULL",
    [kToken_yEnc]      = "yEnc"
};


typedef enum {
    kHashUnset      = 0,
    kHashEmpty      = 0xDeadBeef,

    // elements
    kHashNZB        = 0x000151a90eb474b2,
    kHashSegments   = 0x57ef75389804b12b,
    kHashSegment    = 0x78d5ad74034dd104,
    kHashHead       = 0x003cafa0bdb94552,
    kHashMeta       = 0x003cafa0bd974cbd,
    kHashGroups     = 0x029a347370b2f5eb,
    kHashGroup      = 0x0b18912267fc3ade,
    kHashFile       = 0x003cafa0bdeb36b1,

    // attributes
    kHashXmlns      = 0x0b189122f49400cb,
    kHashType       = 0x003cafa0badc89f9,
    kHashSubject    = 0x78d5adc5d7a0afe2,
    kHashDate       = 0x003cafa0bda6aa31,
    kHashBytes      = 0x0b189122f1c044a3,
    kHashNumber     = 0x029a34715398d358,
    kHashPoster     = 0x029a344bcb84b4a0,

    // subject
    kHashWtFnZb     = 0x029a3476ebecf502,
    kHashPRiVATE    = 0x78d5ad39d3933041,
    kHashN3wZ       = 0x003cafa0bae64024,
    kHashnewzNZB    = 0x78d594be57432922,
    kHashFULL       = 0x003cafa0bd59fc48,
    kHashyEnc       = 0x003cafa0bacd759b,

    // force the enum width to be 64 bits
    kHashForceWidth = 0x8070605040302010
} tHash;

typedef unsigned long tSignature;

typedef struct sAttribute {
    struct sAttribute * next;

    tHash           attributeHash;
    const char *    value;
} tAttribute;

typedef struct sElement {
    struct sElement * next;

    tHash           elementHash;
    tAttribute *    attributes;
    char *          contents;
} tElement;


struct {
    tHash hash;
    const char * string;
} hashAsString[] = {
    // elements
    { kHashNZB,      "kHashNZB"      },
    { kHashSegments, "kHashSegments" },
    { kHashSegment,  "kHashSegment"  },
    { kHashHead,     "kHashHead"     },
    { kHashMeta,     "kHashMeta"     },
    { kHashGroups,   "kHashGroups"   },
    { kHashGroup,    "kHashGroup"    },
    { kHashFile,     "kHashFile"     },

    // attributes
    { kHashXmlns,    "kHashXmlns"    },
    { kHashType,     "kHashType"     },
    { kHashSubject,  "kHashSubject"  },
    { kHashDate,     "kHashDate"     },
    { kHashBytes,    "kHashBytes"    },
    { kHashNumber,   "kHashNumber"   },
    { kHashPoster,   "kHashPoster"   },

    // subject
    { kHashWtFnZb,   "kHashWtFnZb"   },
    { kHashPRiVATE,  "kHashPRiVATE"  },
    { kHashN3wZ,     "kHashN3wZ"     },
    { kHashnewzNZB,  "kHashnewzNZB"  },
    { kHashFULL,     "kHashFULL"     },
    { kHashyEnc,     "kHashyEnc"     },

    { kHashUnset,    "kHashUnset"    },
    { kHashEmpty,    "kHashEmpty"    },
    { 0,             NULL            }
};


tHash hashString( const char * string, const int maxLen )
{
    tHash hash = 0xDeadBeef;
    int remaining = maxLen;
    if (remaining < 1) remaining = (-1 >> 1);
    while ( remaining-- != 0 && *string != '\0') {
        hash ^= (hash * 47 ) + *string++;
    }
    if (hash == 0) hash++;
    return hash;
}

int parseNumber( const char * string, const int maxLen )
{
    int result = 0;
    for (int i = 0; i < maxLen; i++)
    {
        if ( isdigit(string[i]) ) {
            result = result * 10 + (string[i] - '0');
        } else {
            return -1;
        }
    }
    return result;
}

const char * describeHash( tHash hash )
{
    for ( unsigned int i = 0; hashAsString[i].hash != 0; ++i ) {
        if ( hashAsString[i].hash == hash ) {
            return hashAsString[i].string;
        }
    }
}

void trimstr( char * string )
{
    char * end = string;
    for ( char * ptr = string; *ptr != '\0'; ptr++ ) {
        if ( isgraph(*ptr) ) end = &ptr[1];
    }

    *end = '\0';
}

const char * substr( const char * haystack, const char * needle )
{
    const char * result = strstr( haystack, needle );
    if ( result != NULL )
    {
        result += strlen( needle );
    }
    return result;
}


const char * strblock( const char * start, const char * end )
{
    ssize_t length = end - start;
    char * result = NULL;
    if (length > 1) {
        result = malloc( length + 1 );
        if ( result != NULL ) {
            memcpy( result, start, length );
            result[length] = '\0';
        }
    }
    return (const char *)result;
}

void setDirectory(const char * directory, int length )
{
    printf("directory \'%.*s\'\n", length, directory);
}

void setFilename(const char * filename, int length )
{
    printf("filename \'%.*s\'\n", length, filename);
}

#if 0
void processBracketed( const char *start, const char *end )
{
    const char * p;
    const char * s;
    const char * lastSep;
    long         index;
    long         total;
    bool         allDigits;
    int          number;

    index = 0;
    allDigits = true;
    lastSep = NULL;
    s = start;
    for ( p = start; *p != '\0'; p++ ) {
        if ( *p == '/' ) {
            lastSep = p;
            switch ( index ) {
                case 0: // up to the first sep
                    break;

                case 1: // after the first sep
                    break;

                default:
                    /* ignore further slashes, e.g. in a path */
                    break;

            }
            s = p + 1;
            index++;
            allDigits = true;
        } else if (isdigit(*p)) {
            number = number * 10 + (*p - '0');
        } else {
            allDigits = false;
        }
    }

    if (*start == '/') {
        const char * p = start + 1;
        const char * sep = p;
        while (p < end)
        {
            if (*p == '/') {
                sep = p + 1;
            }
            p++;
        }
        if ( sep != start + 1) {
            setDirectory( start + 1, ( sep - start - 2));
        }
        setFilename( sep, (end - sep));
    } else if (isdigit(*start)) {
        // check for [123], [12/123], or [12/file.ext]
        p = start;
        index = 0;
        while (p < end && isdigit(*p)) {
            index = index * 10 + (*p - '0');
            p++;
        }
        if (*p == '/') {
            p++;
            total = 0;
            while (p < end && isdigit(*p)) {
                total = total * 10 + (*p - '0');
                p++;
            }
            if (p < end) {
                // not all digits after the slash, filename?
                printf("index = %ld\n", index);
                setFilename(p, (end - p));
            } else {
                // all digits after the slash, so assume it's the total
                printf("index = %ld, total = %ld\n", index, total);
            }
        } else {
            printf("all digits: %ld\n", index);
        }
    } else {
        tHash hash = hashString( start, end - start );
        switch (hash)
        {
        case kHashFULL:
            printf("### FULL\n");
            break;

        case kHashN3wZ:
            printf("### N3wZ\n");
            break;

        case kHashPRiVATE:
            printf("### PRiVATE\n");
            break;

        case kHashWtFnZb:
            printf("### WtFNzB\n");
            break;

        default:
            printf("### unhandled: \'%.*s\' %s\n", (int)(end - start), start, describeHash(hash));
            break;
        }
    }
}


bool processToken( tTokenType tokenType, const char *start, const char *end )
{
    tHash hash;
    const char * p;
    const char * filename = NULL;
    long index;
    long total;

    // trim spaces and dashes from the beginning and end
    while ( start < end && (*start == ' ' || *start == '-') ) { start++; }
    do { --end; } while ( start < end && (*end == ' ' || *end == '-') );
    end++;

    if ( (end - start) > 0 ) {
        printf( "%s: \'%.*s\'\n", tokenTypeNames[tokenType], (int)(end - start), start );

        switch ( tokenType )
        {
        case kToken_Unquoted:
            if ( strncmp( start, "yEnc", 4 ) == 0 ) {
                printf( "### yEnc\n" );
                // ignore the rest
                return true;
            }
            break;

        case kToken_String:
            processBracketed( start, end );
            break;

        case kToken_Quoted:
            setFilename( start, (int)(end - start) );
            return true;

        default:
            break;
        }
    }
#ifdef DEBUG_VERBOSE
    else {
        printf( "%s: (empty)\n", tokenTypeNames[tokenType] );
    }
#endif
    return false;
}
#endif

tSignature updateSignature( tSignature signature, tHash hash )
{
    signature <<= 4;
    switch ( hash ) {
    case kHashEmpty:
        signature |= kToken_Empty; break;
    case kHashN3wZ:
        signature |= kToken_N3wZ; break;
    case kHashnewzNZB:
        signature |= kToken_newzNZB; break;
    case kHashWtFnZb:
        if ( ((signature >> 4) & 0x0f) == kToken_PRiVATE ) {
            signature = ((signature >> 4) & ~0x0f) | kToken_PRiV_WtF;
        } else {
            signature |= kToken_WtFnZb;
        }
        break;

    case kHashPRiVATE:
        signature |= kToken_PRiVATE; break;
    case kHashFULL:
        signature |= kToken_FULL; break;
    case kHashyEnc:
        signature |= kToken_yEnc; break;

    default:
        signature |= kToken_String;
        break;
    }
    printf( "\n%s", tokenTypeNames[signature & 0x0F]);
    return signature;
}

/*
  Observed Variations of the subject field:

  "what.on.earth.s07e01.nazi.doomsday.forest.1080p.web.x264-caffeine.vol007-015.par2" yEnc (01/17)
  "37ad517c81174b98c19c20f498de31f7" yEnc (001/205)
  5e0c74c3d8a34c5080cbc4834b3d392c [1/40] "5e0c74c3d8a34c5080cbc4834b3d392c.par2" yEnc (1/1)
  [01/57] - "9ciQK4R3mMmKGyhEXWTqlj.par2" yEnc (1/1) 51264
  [ You.Cant.Turn.That.Into.A.House.S01E01.720p.FYI.WEB-DL.AAC2.0.H.264-BOOP ] - "You.Cant.Turn.That.Into.A.House.S01E01.Industrial.Storage.Tank.720p.FYI.WEB-DL.AAC2.0.H.264-BOOP.part01.rar" yEnc (01/66)
  [PRiVATE]-[WtFnZb]-[4]-[3/WtF[nZb].nfo] - "" yEnc (1/[PRiVATE] \7768e6b602\::6f9560c7de0d6e.22969e532ecbcc207eb8675e6cd357.b9de684f::/4f8df0724783/) 1 (1/0) (1/0)
  [PRiVATE]-[WtFnZb]-[Underground.Marvels.S01E01.Secrets.of.the.Rock.720p.WEBRip.x264-CAFFEiNE.mkv]-[1/7] - "" yEnc  1022215424 (1/1997)
  [PRiVATE]-[WtFnZb]-[/Whose.Line.Is.It.Anyway.US.S20E03.1080p.WEB.h264-EDITH[rarbg]/Whose.Line.Is.It.Anyway.US.S20E03.1080p.WEB.h264-EDITH.mkv]-[1/2] - "" yEnc  1279019728 (1/1785)
  [N3wZ] \bdIcha192688\::[PRiVATE]-[WtFnZb]-[4]-[1/Chuck.S02E01.Chuck.Versus.the.First.Date.REPACK.1080p.BluRay.REMUX.VC-1.DD5.1-EPSiLON.mkv] - "" yEnc (1/[PRiVATE] \2ad4205f2d\::ec1ad90d6f098c.ab185e364cea90e285f350352733d6.fcdf2877::/bec21662f1e5/) 1 (1/0) (1/0)
  [145943]-[FULL]-[#a.b.teevee]-[ FantomWorks.S01E01.1963.Corvette.and.1931.Model.A.Hot.Rod.720p.HDTV.x264-DHD ]-[01/44] - "fantomworks.s01e01.1963.corvette.and.1931.model.a.hot.rod.720p.hdtv.x264-dhd-sample.mkv" yEnc (1/43)

*/

void processSubject( const unsigned char * subject )
{
    static struct {
        unsigned char matchChar;
        unsigned int  isSeparator : 1;
        unsigned int  ifUnquoted  : 1;
        unsigned int  isBeginRun  : 1;
        unsigned int  isEndRun    : 1;
    } charMap[256] = {
            [0]   = {0,   1, 1, 0, 1},
            ['"'] = {'"', 1, 1, 1, 1},
            ['['] = {']', 1, 1, 1, 0},
            [']'] = {0,   1, 1, 0, 1},
            ['('] = {')', 1, 1, 1, 0},
            [')'] = {0,   1, 1, 0, 1},
            ['-'] = {0,   1, 0, 0, 0},
            [' '] = {0,   1, 0, 0, 1}
    };

    unsigned char debug[5][1024];
    const unsigned char * p;

    tSignature signature = kTokenTypeMax;
    tHash hash = 0xDeadBeef;

    printf("\nS: %s\n", subject);

    const unsigned char * start = NULL;
    const unsigned char * end;

    memset(debug, '\0', sizeof(debug));

    unsigned char matchChar;
    unsigned int level = 0;
    bool newState = false;
    bool prevState = true;

    bool abort = false;
    for ( p = subject; *p != '\0' && !abort; p++ )
    {
        unsigned int i = (unsigned int)(p - subject);
        if ( i >= sizeof(debug[0]) ) i++;

        debug[0][i] = *p;
        debug[1][i] = charMap[*p].isSeparator ? '^' : '_';
        debug[2][i] = matchChar;
        debug[3][i] = ' ';
        debug[4][i] = (unsigned char)level + '0';

        newState = charMap[*p].isSeparator;
        if (newState != prevState ) {
            if ( newState ) {
                end = p;
            } else if (start == NULL) {
                start = p;
            }
            prevState = newState;
        }

        if ( charMap[*p].isSeparator ) {
            if (*p == matchChar) {
                if (level > 0) {
                    --level;
                    if (level == 0) debug[1][i] = '}';
                }
                if (level == 0 && start != NULL) {
                    signature = updateSignature(signature, hash);

                    debug[3][start - subject] = 'S';
                    debug[3][  end - subject] = 'E';

                    printf(" \'%.*s\' (0x%lx) 0x%lx", (int) (end - start), start, hash, signature);

                    if ((signature & 0x0f) == kToken_yEnc) {
                        printf("\nparsing stopped");
                        abort = true;
                    }

                    matchChar = ' ';
                    start = NULL;
                }
            } else if (charMap[*p].isBeginRun) {
                debug[2][i] = *p;
                if (level == 0) {
                    hash = 0xDeadBeef;
                    matchChar = charMap[*p].matchChar;
                }
                if (matchChar == charMap[*p].matchChar) {
                    ++level;
                    if (level == 1) debug[1][i] = '{';
                }
            }
        } else {
            hash ^= (hash * 47) + *p;
        }

    }

    printf( "\nP: %s\nT: %s\nR: %s\nQ: %s\nL: %s\n", debug[0], debug[1], debug[2], debug[3], debug[4] );
    printf( "sig: 0x%lx\n", signature );
}

#if 0
{
    const int kIsSeparator    = 0b00000001;
    const int kIsTerminator   = 0b00000010;
    const int kIsQuote        = 0b00000100;
    const int kIsLeftBracket  = 0b00001000;
    const int kIsRightBracket = 0b00010000;

    static unsigned char tokenSepMap[256] = {
        [ 0 ] = kIsTerminator,
        ['-'] = kIsSeparator,
        [' '] = kIsSeparator,
        [':'] = kIsSeparator | kIsTerminator,
        ['"'] = kIsSeparator | kIsTerminator | kIsQuote,

        ['<'] = kIsSeparator | kIsLeftBracket,
        ['('] = kIsSeparator | kIsLeftBracket,
        ['['] = kIsSeparator | kIsLeftBracket,

        ['>'] = kIsSeparator | kIsTerminator | kIsRightBracket,
        [')'] = kIsSeparator | kIsTerminator | kIsRightBracket,
        [']'] = kIsSeparator | kIsTerminator | kIsRightBracket
    };

    unsigned char debug[2][1024];

    const unsigned char * p = subject;
    const unsigned char * start;
    const unsigned char * end;
    int                   level = 0;
    unsigned long         signature = kTokenTypeMax;

    bool abort = false;
    bool seenQuote;
    bool isQuoted = false;
    bool isEmpty;
    bool isNumber;
    do {
        start = p;
        if ( tokenSepMap[*p] & kIsSeparator ) {
            seenQuote = false;
            isQuoted = false;
            isEmpty = false;
            do {
                if ( tokenSepMap[*p] & kIsQuote ) {
                    seenQuote = true;
                    isQuoted  = true;
                }
                ++p;
                if ( seenQuote && (tokenSepMap[*p] & kIsQuote)) {
                    isEmpty = true;
                }
                seenQuote = false;
            } while ( tokenSepMap[*p] & kIsSeparator );

            if (isQuoted) {
                signature <<= 4;
                if (isEmpty) {
                    signature |= kToken_Empty;
                    printf( "empty:" );
                }
                else
                {
                    signature |= kToken_Quoted;
                    printf( "quoted:" );
                }
            } else {
                printf( "separator:" );
            }
        } else {
            end = p;
            signature <<= 4;
            tHash hash = 0xDeadBeef;
            isNumber = true;
            level = 0;
            int i = 0;
            do {
                hash ^= (hash * 47) + *p;
                isNumber = isNumber && isdigit( *p );
                if ( tokenSepMap[*p] & kIsLeftBracket )  level++;
                if ( tokenSepMap[*p] & kIsRightBracket ) level--;

                if ( tokenSepMap[*p] & kIsSeparator ) end = p;
                debug[0][i] = *p;
                debug[1][i] = (unsigned char)level + '0';

                p++;
                i = (i+1) % sizeof(debug[0]);
            } while ( *p != '\0' && (level > 0 || !(tokenSepMap[*p] & kIsTerminator)) );
            debug[0][i] = '\0';
            debug[1][i] = '\0';

            switch ( hash ) {
            case kHashPRiVATE:
                signature |= kToken_PRiVATE; break;
            case kHashN3wZ:
                signature |= kToken_N3wZ; break;
            case kHashnewzNZB:
                signature |= kToken_newzNZB; break;
            case kHashWtFnZb:
                signature |= kToken_WtFnZb; break;
            case kHashFULL:
                signature |= kToken_FULL; break;
            case kHashyEnc:
                signature |= kToken_yEnc; abort = true; break;

            default:
                if (isNumber)
                    signature |= kToken_Number;
                else
                    signature |= kToken_String;
                break;
            }
            printf( "%s\n%s\n", debug[0], debug[1] );
            printf( "%s: (0x%lx)", tokenTypeNames[ signature & 0x0F ], hash );
        }
        printf( " \'%.*s\'\n", (int)(p - start), start );
        if (abort) printf("abort\n");
    } while (*p != '\0' && !abort);

//    if (signature > 0x9000000000000000) {
        printf( "sig: 0x%lx\t%s\n", signature, subject );
//    }
}

void processSubject2( const char * subject )
{
    int   level     = 0;
    tHash hash      = 0xDeadBeef;
    bool  allDigits = true;
    int   number    = 0;

    struct {
        unsigned int    count;
        struct {
            const char *  string;
            int           length;
        } list[10];
    } parm;

    static struct {
        unsigned long   signature;
        int             filename;
        int             index;
        int             max;
    } patterns[] = {
        {0xc2,0, -1, -1},
        {0xc62,0,-1,-1 },
        {0xc262, 1, -1, -1 },
        {0xc4162, 1, -1, -1 },
        {0xc5562, 2, 0, 1 },
        {0xc6552, 0, 1, 2 },
        {0xc9262, 1, -1, -1 },
        {0xc25562, 3, 1, 2 },
        {0xc55162, 2, 0, 1 },
        {0xc255162, 3, 1, 2 },
        {0xc82a442, 0, 1, 2 },
        {0xc82a552, 0, 1, 2 },
        {0xc925562, 3, 1, 2 },
        {0xc4144162, 3, 1, 2 },
        {0xc4155162, 3, 1, 2 },
        {0xc4255162, 4, 2, 3 },
        {0xc5155162, 3, 1, 2 },
        {0xc9255162, 3, 1, 2 },
        {0xc41455162, 4, 2, 3 },
        {0xc9282a552, 1, 2, 3 },
        {0xc414144162, 4, 2, 3 },
        {0xc424255162, 6, 4, 5 },
        {0xc514155162, 4, 2, 3 },
        {0xc817141552, 0, 1, 2},
        {0xc924155162, 4, 2, 3 },
        {0xc2524155162, 6, 4, 5 },
        {0xc4141455162, 5, 3, 4 },
        {0xc41414155162, 5, 3, 4 },
        {0xc81714155132, 0 ,1, 2 },
        {0xc81715154132, 2, 1, 0 },
        {0xc92514155162, 5, 3, 4 },
        {0xc92817141552, 1, 2, 3 },
        {0xc414141455162, 6, 4, 5 },
        {0xc51a1414155162, 5, 3, 4 },
        {0xc9281714155132, 1, 2, 3 },
        {0xc9281715154132, 3, 2, 1 },
        // also have path or container name
        {0xc6162, 1, -1, -1 },
        {0xc425562, 4, 2, 3 },
        {0xc525562, 4, 2, 3 },
        {0xc2455162, 4, 2, 3 },
        {0xc4155162, 3, 1, 2 },
        {0xc42455162, 5, 3, 4 },
        {0xc414155162, 4, 2, 3 },
        {0xc924255162, 5, 3, 4 },
        {0xc5141455162, 5, 3, 4 },
        {0xc51b1414155162, 5, 3, 4 },
        {0xc8171544155132, 3, 4, 5 },
        {0xc81715444155132, 3, 4, 5 },
        {0xc9251a1414155162, 6, 4, 5 },
        {0xc9251b1414155162, 6, 4, 5 },
        {0xc928171544155132, 3, 4, 5 },
        // encoded?
        {0xc92, 0, -1, -1 },
        {0xc827552, 0, 1, 2 },
        {0xc824552, 0, 2, 3},
        {0xc92827552, 1, 2, 3 },
        // table end
        {0,-1,-1,-1}
    };

    parm.count = 0;
    printf("\nsubject: \'%s\'\n", subject);

    unsigned long signature = kTokenTypeMax;

    const char * p     = subject;
    const char * start = p;

    do {
        // handle the unquoted run when we detect the beginning of a quoted run or end-of-string
        if ((level == 0) && (p > start) && (*p == '\"' || *p == '[' || *p == '\0')) {
            while (*start != '\0' && isblank(*start)) { start++; }
            const char *end = p;
            while (end > start && isblank(end[-1])) { end--; }

            switch ( end - start ) {
            case 0:
                break;

            case 1:
                if ( *start == '-' ) {
                    signature = (signature <<= 4) | kToken_Separator;
                    // printf( "separator\n" );
                } else {
            default:
                    signature = (signature <<= 4) | kToken_Unquoted;
                    if (strncmp(start, "yEnc", 4) == 0) {
                        // don't bother processing the rest of the string
                        while ( *p != '\0' ) {
                            ++p;
                        }
                    } else {
                        parm.list[parm.count].string = start;
                        parm.list[parm.count].length = (end - start);
                        ++parm.count;
                        // printf("unquoted: \'%.*s\'\n", (int) (end - start), start );
                    }
                }
                break;
            }
        }

        switch ( *p )
        {
        case '\0':
            break;

        case '\"':
            p++;
            start = p;
            // scan through double-quoted run
            while ( *p != '"' && *p != '\0' ) {
                p++;
            }

            signature <<= 4;
            if ( p == start ) {
                signature |= kToken_Empty;
                // printf( "empty quotes\n" );
            } else {
                signature |= kToken_Quoted;
                parm.list[parm.count].string = start;
                parm.list[parm.count].length = (p - start);
                ++parm.count;
                // printf( "quoted: \"%.*s\"\n", (int) (p - start), start );
            }
            start = p + 1;
            break;

        case '[':
            level++;
            hash = 0xDeadBeef;
            // consume any leading whitespace
            do { ++p; } while ( *p != '\0' && isblank(*p));
            start     = p;
            allDigits = true;
            number    = 0;

            while ( *p != '\0' && level > 0 )
            {
                switch ( *p )
                {
                case '[':
                    // handle nested square brackets
                    level++;
                    break;

                case '/':
                    // handle embedded slashes in bracketed runs

                    signature <<= 4;
                    number = parseNumber( start, (p - start) );
                    if ( number < 0 ) {
                        signature |= kToken_String;

                        parm.list[parm.count].string = start;
                        parm.list[parm.count].length = (p - start);

                        // printf( "string: \'%.*s\'\n", (int) (end - start), start );
                    } else {
                        signature |= kToken_Number;

                        parm.list[parm.count].string = NULL;
                        parm.list[parm.count].length = number;

                        // printf( "number: %d\n", number );
                    }
                    ++parm.count;

                    // reset for the next run
                    hash      = 0xDeadBeef;
                    allDigits = true;
                    number    = 0;
                    for ( start = p + 1; *start != '\0' && isblank(*start); start++ ) { /* spin */ }
                    break;

                case ']':
                    // handle nested square brackets
                    --level;
                    if ( level <= 0 ) {
                        // end of square-bracketed run
                        level = 0;

                        signature <<= 4;
                        switch ( hash )
                        {
                        case kHashPRiVATE:  signature |= kToken_PRiVATE; break;
                        case kHashN3wZ:     signature |= kToken_N3wZ;    break;
                        case kHashnewzNZB:  signature |= kToken_newzNZB; break;
                        case kHashWtFnZb:   signature |= kToken_WtFnZb;  break;
                        case kHashFULL:     signature |= kToken_FULL;    break;

                        default:
                            while (*start != '\0' && isblank(*start)) { start++; }
                            const char *end = p;
                            while (end > start && isblank(end[-1])) { end--; }

                            number = parseNumber( start, (end - start) );
                            if ( number < 0 ) {
                                signature |= kToken_String;

                                parm.list[parm.count].string = start;
                                parm.list[parm.count].length = (end - start);

                                // printf( "string: \'%.*s\'\n", (int) (end - start), start );
                            } else {
                                signature |= kToken_Number;

                                parm.list[parm.count].string = NULL;
                                parm.list[parm.count].length = number;

                                // printf( "number: %d\n", number );
                            }
                            ++parm.count;

                            break;
                        }

                        // reset for next run
                        hash      = 0xDeadBeef;
                        allDigits = true;
                        number    = 0;
                        start     = p + 1;
                        for ( start = p + 1; *start != '\0' && isblank(*start); start++ ) { /* spin */ }
                    }
                    break;

                default:
                    // normal character, run per-char calcs
                    hash ^= (hash * 47) + *p;
                    break;
                } // switch
                p++;
            } // while
            break;
        } // switch
    } while (*p++ != '\0');

    unsigned int i = 0;
    while ( patterns[i].signature != 0 ) {
        if ( patterns[i].signature == signature ) {
            printf( "matched: 0x%lx, \'%.*s\', ",
                    patterns[i].signature,
                    parm.list[patterns[i].filename].length,
                    parm.list[patterns[i].filename].string );
            if ( patterns[i].index < 0 ) {
                printf( "-, " );
            } else {
                printf( "%d, ", parm.list[patterns[i].index].length );
            }
            if ( patterns[i].max < 0 ) {
                printf( "-\n" );
            } else {
                printf( "%d\n", parm.list[patterns[i].max].length );
            }
            break;
        }
        i++;
    }
    if ( patterns[i].signature == 0 ) {
        printf( "sig: 0x%lx\n", signature );
        for ( int i = 0; i < parm.count; ++i ) {
            if ( parm.list[i].string == NULL ) {
                printf("[%d] %d\n", i, parm.list[i].length);
            } else {
                printf("[%d] %.*s (0x%016lx)\n",
                       i,
                       parm.list[i].length,
                       parm.list[i].string,
                       hashString( parm.list[i].string, parm.list[i].length) );
            }
        }
    }
}
#endif

void processElement( tElement * element )
{
#ifdef DEBUG_VERBOSE
    if ( element != NULL && element->elementHash != kHashSegment ) {
        printf( "   >  %s: \'%s\'\n", describeHash(element->elementHash), element->contents );

        for ( tAttribute * attribute = element->attributes;
              attribute != NULL;
              attribute = attribute->next )
        {
            printf( "     >  %s: \'%s\'\n", describeHash(attribute->attributeHash ), attribute->value );
        }
    }
#endif
}

int processFile( FILE * file )
{
    yxml_t      xml;
    yxml_ret_t  r = YXML_OK;
    char        buffer[4096];
    char        value[1024];

    int level = 0;
    yxml_init( &xml, buffer, sizeof( buffer ) );

    tElement *    element   = NULL;
    tAttribute *  attribute = NULL;
    tElement *    newElement;
    int ch;
    while( (ch = fgetc( file )) != EOF  )
    {
        r = yxml_parse( &xml, ch );
        switch ( r )
        {
        case YXML_ELEMSTART:
#ifdef DEBUG_VERBOSE
            printf( "%d  ElemStart %s = 0x%016lx\n", level, xml.elem, hashString( xml.elem ) );
#endif
            if ((newElement = calloc( 1, sizeof( tElement ))) != NULL) {
                newElement->elementHash = hashString( xml.elem, 0 );

                /* push new entry on the element stack */
                newElement->next = element;
                element = newElement;
            }
            level++;
            value[ 0 ] = '\0';
            break;

        case YXML_ATTRSTART:
#ifdef DEBUG_VERBOSE
            printf( "   AttrStart %s = 0x%016lx\n", xml.attr, hashString( xml.attr ) );
#endif
            if ( (attribute = calloc( 1, sizeof( tAttribute ))) != NULL ) {
                attribute->attributeHash = hashString( xml.attr, 0 );
            }
            attribute->next     = element->attributes;
            element->attributes = attribute;
            value[ 0 ] = '\0';
            break;

        case YXML_ATTRVAL:
        case YXML_CONTENT:
            strncat( value, xml.data, sizeof(value) - 2 );
            break;

        case YXML_ATTREND:
#ifdef DEBUG_VERBOSE
            printf( "   AttrEnd %s \'%s\'\n", xml.attr, value );
#endif
            attribute->value = strdup( value );
            if ( element->elementHash == kHashFile && attribute->attributeHash == kHashSubject ) {
                processSubject( attribute->value );
            }

            value[ 0 ] = '\0';
            break;

        case YXML_ELEMEND:
            --level;
#ifdef DEBUG_VERBOSE
            printf( "%d  ElemEnd %s \'%s\'\n", level, xml.elem, value );
#endif
            if ( element != NULL ) {
                void * temp;

                trimstr(value);
                if ( strlen( value ) > 0 ) {
                    element->contents = strdup( value );
                    value[ 0 ] = '\0';
                }

                processElement( element );

                // release attributes assigned to the element at the top of the element stack
                attribute = element->attributes;
                while ( attribute != NULL ) {
                    temp      = attribute;
                    attribute = attribute->next;
                    free( temp );
                }
                // 'pop' the top of the element stack, and release its memory
                temp    = element;
                element = element->next;
                free( temp );
            }
            break;

        default:
            if ( r < 0 ) {
                fprintf( stderr, "xml parser error %d\n", r );
                exit( r );
            }
            break;
        }
    }

    r = yxml_eof( &xml );
    if ( r != YXML_OK ) {
        fprintf( stderr, "xml error %d at end of file", r );
    }

    return r;
}

int main( int argc, const char *argv[] )
{
    const char * myName = strrchr(argv[0], '/');
    if ( myName++ == NULL ) {
        myName = argv[0];
    }

    if ( argc < 2 ) {
        processFile(stdin);
    } else {
        for ( int i = 1; i < argc; ++i )  {
            FILE * file = fopen( argv[i], "r" );
            if ( file == NULL ) {
                fprintf( stderr,
                         "### %s: error: unable to open \'%s\' (%d: %s)\n",
                         myName, argv[i], errno, strerror(errno) );
                exit(-errno);
            } else {
                processFile( file );
                fclose( file );
            }
        }
    }

    return 0;
}
