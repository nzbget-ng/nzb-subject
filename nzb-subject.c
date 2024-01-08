
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "yxml.h"

// #define DEBUG_VERBOSE 1
//#undef DEBUG

#if DEBUG
#define logDebug(...)    fprintf(stderr, __VA_ARGS__ )
#else
#define logDebug( ... )    do {} while (0)
#endif

typedef unsigned char byte;

static struct {
    enum eRunEndType { kNotEnd = 0, kSeparator, kDoubleQuotes, kLeftSquareBracket, kRightSquareBracket } runEndType;
} charMap[256] = {
        ['\0'] = { kSeparator },
        [' ']  = { kSeparator },
        ['-']  = { kSeparator },
        ['"']  = { kDoubleQuotes },
        ['[']  = { kLeftSquareBracket },
        [']']  = { kRightSquareBracket }
};

const char * runEndTypeAsString[] = {
    [kNotEnd]             = "not end",
    [kSeparator]          = "seperator",
    [kDoubleQuotes]       = "quoted",
    [kLeftSquareBracket]  = "[",
    [kRightSquareBracket] = "]"
};


typedef enum {
    kSep_nop = 0,             // ''
    kSep_startSq,             // '[', '[ ', ' [', ' [ '
    kSep_endSq,               // ']', '] ', ']  '
    kSep_dash,                // ' -', ' - ', ' -  ', '--'
    kSep_endBracket,          // ')', ') '
    kSep_startBracket,        // '(', ' (', '  ('
    kSep_emptyQuotes,         // '""'
    kSep_startQuotes,         // ' "',
    kSep_endQuotes,           // '" ', '"  '
    kSep_space                // ' ', '  '
} tSeparatorIndex;

const char * sepTokenToString[] = {
        [kSep_nop]                = "-",
        [kSep_startSq]            = "(start square)",
        [kSep_endSq]              = "(end square)",
        [kSep_dash]               = "(dash)",
        [kSep_endBracket]         = "(end bracket)",
        [kSep_startBracket]       = "(start bracket)",
        [kSep_emptyQuotes]        = "(empty quotes)",
        [kSep_startQuotes]        = "(start quotes)",
        [kSep_endQuotes]          = "(end quotes)",
        [kSep_space]              = "(space)"
};

struct {
    unsigned long hash;
    tSeparatorIndex index[2];
} separatorHashTable[] = {
        { 0x00000000deadbeef, {}},                                           // ''
//        { 0x000000283f4bb0d1, { kSep_endSq, kSep_nop } },                            // ']'
//        { 0x000000283f4bb0d3, { kSep_startSq, kSep_nop } },                          // '['
//        { 0x000000283f4bb0e1, { kSep_dash.0 } },                             // '-'
//        { 0x000000283f4bb0e5, { kSep_endBracket} },                        // ')'
//        { 0x000000283f4bb0e6, { kSep_startBracket} },                      // '('
        { 0x000000283f4bb0ec, { kSep_endQuotes,  kSep_startQuotes }},                      // '"'
        { 0x000000283f4bb0ee, { kSep_space }},                            // ' '
        { 0x0000074ba1aec60e, { kSep_startSq }},                          // '[ '
        { 0x0000074ba1aec65d, { kSep_endSq }},                            // ']-'
        { 0x0000074ba1aec66b, { kSep_endSq,      kSep_startSq }},           // ']['
        { 0x0000074ba1aec6ae, { kSep_endSq }},                            // '] '
        { 0x0000074ba1aec94b, { kSep_startSq }},                          // '-['
        { 0x0000074ba1aec99d, { kSep_dash }},                             // '--'
        { 0x0000074ba1aecace, { kSep_endBracket }},                       // ') '
        { 0x0000074ba1aecadd, { kSep_endBracket }},                       // ')-'
        { 0x0000074ba1aecb31, { kSep_dash }},                             // ' -'
        { 0x0000074ba1aecb34, { kSep_startBracket }},                     // ' ('
        { 0x0000074ba1aecb3a, { kSep_startQuotes }},                      // ' "'
        { 0x0000074ba1aecb3c, { kSep_space }},                            // '  '
        { 0x0000074ba1aecb98, { kSep_endQuotes }},                        // '" '
//      { 0x0000074ba1aecc18, { } },                                           // ': '
        { 0x0000074ba1aecce3, { kSep_startSq }},                          // ' ['
//      { 0x0000074ba1aecdf2, { } },                                           // '::'
        { 0x000151a90eb8ad33, { kSep_endSq,      kSep_startSq }},                // ']-['
        { 0x000151a90eb8ad68, { kSep_endSq,      kSep_startQuotes }},       // ']-"'
        { 0x000151a90eb8bcba, { kSep_endSq,      kSep_startQuotes }},       // '] "'
        { 0x000151a90eb8bcbc, { kSep_endSq }},                            // ']  '
        { 0x000151a90eb8bce3, { kSep_endSq,      kSep_startSq }},          // '] ['
        { 0x000151a90eb9023b, { kSep_startSq }},                          // '::['
        { 0x000151a90eb90264, { kSep_startBracket }},                     // '::('
        { 0x000151a90eb9512e, { kSep_startSq }},                          // ' [ '
//        { 0x000151a90eb9852e, { kSep_dash } },                             // ' - '
        { 0x000151a90eb9856b, { kSep_startSq }},                          // ' -['
        { 0x000151a90eb99b10, { kSep_startBracket }},                     // '  ('
        { 0x000151a90eb9aa88, { kSep_endQuotes,  kSep_startBracket }},  // '" ('
        { 0x000151a90eb9aa90, { kSep_endQuotes }},                        // '"  '
        { 0x000151a90eb9aadb, { kSep_endQuotes,  kSep_startSq }},          // '" ['
//        { 0x000151a90eb9b492, { kSep_colon, kSep_startQuotes } },          // ': "'
        { 0x000151a90eb9c8e3, { kSep_dash,       kSep_startSq }},                          // '- ['
        { 0x000151a90eb9f134, { kSep_endBracket, kSep_startBracket }},    // ') ('
        { 0x000151a90eb9f13a, { kSep_endBracket }},                       // ') "'
        { 0x000151a90eb9f6e3, { kSep_endBracket, kSep_startSq }},         // ') ['
        { 0x003cafa0baa5e0e3, { kSep_dash,       kSep_startSq }},                          // '-- ['
        { 0x003cafa0baaffa8e, { kSep_endQuotes }},                        // '" - '
        { 0x003cafa0bab6f6ba, { kSep_startQuotes }},                      // ' - "'
        { 0x003cafa0bab6f6bc, { kSep_dash }},                             // ' -  '
        { 0x003cafa0bab6f6e3, { kSep_dash,       kSep_startSq }},            // ' - ['
        { 0x003cafa0babca8e3, { kSep_endSq,      kSep_startSq }},              // ' ] ['
        { 0x003cafa0babcadb3, { kSep_endSq,      kSep_startSq }},                // ' ]-['
        { 0x003cafa0bd4e6ace, { kSep_startSq }},                          // '::[ '
        { 0x003cafa0bd52182e, { kSep_endSq }},                            // '] - '
        { 0x003cafa0bd521a3b, { kSep_endSq,      kSep_startQuotes }},          // '] "['
        { 0x003cafa0bd521ae9, { kSep_endSq,      kSep_startQuotes }},          // '] "-'
//        { 0x003cafa0bd5e4133, { } },                              // ']]-['
        { 0x003cafa0bd5f614e, { kSep_endSq,      kSep_startSq }},                // ']-[ '
        { 0x003cafa0bd630eb4, { kSep_startBracket }},                     // '[[ ('
        { 0x029a3448818ee8ba, { kSep_endSq,      kSep_startQuotes }},          // ' ] - "'
        { 0x029a3448818ee8e3, { kSep_endSq,      kSep_startSq }},                // ' ] - ['
//        { 0x029a344e2ef57a0d, { } },                              // '--:-:-'
//        { 0x029a3476db94a29d, { } },                              // '-:-:--'
        { 0x029a34772393523b, { kSep_endSq,      kSep_startQuotes }},          // '] - "['
        { 0x029a347723a67aba, { kSep_endSq,      kSep_startQuotes }},          // ']  - "'
        { 0x0b1891227f4068ba, { kSep_endSq,      kSep_startQuotes }},          // '] - "'
        { 0x0b1891227f4068e3, { kSep_endSq,      kSep_startSq }},                // '] - ['
        { 0x0b1891227f40ea1d, { kSep_endSq,      kSep_startQuotes }},          // '] "--'
        { 0x0b189122f21f4e4e, { kSep_endSq,      kSep_startSq }},                // ' ]-[ '
        { 0x0b189122fce0fab4, { kSep_endQuotes,  kSep_startBracket }},     // '" - ('
        { 0x0b189122fce0faba, { kSep_endQuotes,  kSep_startQuotes }},      // '" - "'
        { 0x0b189122fd21ba1a, { kSep_startQuotes }},                      // ' -  "'

        { 0x78d595a8ab9f687c, { kSep_endSq,      kSep_emptyQuotes }},          // '] - "" '
        { 0x78d5ad0748b2292e, { kSep_endSq,      kSep_startSq }},                // ' ] - [ '
        { 0,                  {}}
};

typedef enum {
    kToken_Unset = 0,
    kToken_Separator, // 1
    kToken_Unquoted,  // 2
    kToken_Quoted,    // 3
    kToken_Empty,     // 4
    kToken_String,    // 5
    kToken_Number,    // 6
    kToken_Fraction,  // 7
    kToken_WtFnZb,    // 8
    kToken_PRiVATE,   // 9
    kToken_N3wZ,      // a
    kToken_newzNZB,   // b
    kToken_FULL,      // c
    kToken_yEnc,      // d
    kToken_Of,        // e
    kTokenTypeMax     // f
} tTokenType;


static const char * tokenTypeNames[kTokenTypeMax] = {
        [kToken_Unset]     = "unset",
        [kToken_Separator] = "separator",
        [kToken_Unquoted]  = "unquoted",
        [kToken_Quoted]    = "quoted",
        [kToken_Empty]     = "empty",
        [kToken_String]    = "string",
        [kToken_Number]    = "number",
        [kToken_Fraction]  = "fraction",
        [kToken_WtFnZb]    = "WtFnZb",
        [kToken_PRiVATE]   = "PRiVATE",
        [kToken_N3wZ]      = "N3wZ",
        [kToken_newzNZB]   = "newzNZB",
        [kToken_FULL]      = "FULL",
        [kToken_yEnc]      = "yEnc",
        [kToken_Of]        = "of"
};


typedef enum {
    kHash_Unset = 0,
    kHash_Empty = 0xDeadBeef,
    kHash_OneSpace = 0x000000283f4bb0ee, // hash after parsing a single space

    // elements
    kHash_NZB = 0x000151a90eb474b2,
    kHash_Segments = 0x57ef75389804b12b,
    kHash_Segment = 0x78d5ad74034dd104,
    kHash_Head = 0x003cafa0bdb94552,
    kHash_Meta = 0x003cafa0bd974cbd,
    kHash_Groups = 0x029a347370b2f5eb,
    kHash_Group = 0x0b18912267fc3ade,
    kHash_File = 0x003cafa0bdeb36b1,

    // attributes
    kHash_Xmlns = 0x0b189122f49400cb,
    kHash_Type = 0x003cafa0badc89f9,
    kHash_Subject = 0x78d5adc5d7a0afe2,
    kHash_Date = 0x003cafa0bda6aa31,
    kHash_Bytes = 0x0b189122f1c044a3,
    kHash_Number = 0x029a34715398d358,
    kHash_Poster = 0x029a344bcb84b4a0,

    // subject
    kHash_WtFnZb = 0x029a3476ebecf502,
    kHash_PRiVATE = 0x78d5ad39d3933041,
    kHash_N3wZ = 0x003cafa0bae64024,
    kHash_newzNZB = 0x78d594be57432922,
    kHash_FULL = 0x003cafa0bd59fc48,
    kHash_yEnc = 0x003cafa0bacd759b,
    kHash_Of = 0x0000074ba1aec3c8,

    // guarantee the enum width is at least 64 bits
    kHash_ForceWidth = 0x8070605040302010
} tHash;

typedef unsigned long tSignature;

typedef struct sAttribute {
    struct sAttribute * next;

    tHash attributeHash;
    const char * value;
} tAttribute;

typedef struct sElement {
    struct sElement * next;

    tHash elementHash;
    tAttribute * attributes;
    const char * contents;
} tElement;


struct {
    tHash hash;
    const char * string;
} hashAsString[] = {
        // elements
        { kHash_NZB,      "NZB" },
        { kHash_Segments, "Segments" },
        { kHash_Segment,  "Segment" },
        { kHash_Head,     "Head" },
        { kHash_Meta,     "Meta" },
        { kHash_Groups,   "Groups" },
        { kHash_Group,    "Group" },
        { kHash_File,     "File" },

        // attributes
        { kHash_Xmlns,    "Xmlns" },
        { kHash_Type,     "Type" },
        { kHash_Subject,  "Subject" },
        { kHash_Date,     "Date" },
        { kHash_Bytes,    "Bytes" },
        { kHash_Number,   "Number" },
        { kHash_Poster,   "Poster" },

        // subject
        { kHash_WtFnZb,   "WtFnZb" },
        { kHash_PRiVATE,  "PRiVATE" },
        { kHash_N3wZ,     "N3wZ" },
        { kHash_newzNZB,  "newzNZB" },
        { kHash_FULL,     "FULL" },
        { kHash_yEnc,     "yEnc" },
        { kHash_Of,       "Of" },

        { kHash_Unset,    "Unset" },
        { kHash_Empty,    "Empty" },
        { 0, NULL }
};


tHash hashString(const unsigned char * string, const int maxLen) {
    tHash hash = kHash_Empty;
    int remaining = maxLen;
    if ( remaining < 1 ) remaining = (-1 >> 1);
    while ( remaining-- != 0 && *string != '\0' ) {
        hash ^= (hash * 47) + *string++;
    }
    if ( hash == 0 ) hash++;
    return hash;
}

int parseInteger(const unsigned char * string, const int maxLen) {
    int result = 0;
    for ( int i = 0; i < maxLen; i++ ) {
        if ( isdigit(string[ i ])) {
            result = result * 10 + (string[ i ] - '0');
        } else {
            return -1;
        }
    }
    return result;
}

const char * describeHash(tHash hash) {
    for ( unsigned int i = 0; hashAsString[ i ].hash != 0; ++i ) {
        if ( hashAsString[ i ].hash == hash ) {
            return hashAsString[ i ].string;
        }
    }
}

void trimstr(unsigned char * string) {
    unsigned char * end = string;
    for ( unsigned char * ptr = string; *ptr != '\0'; ptr++ ) {
        if ( isgraph(*ptr)) end = &ptr[ 1 ];
    }

    *end = '\0';
}

const unsigned char * substr(const unsigned char * haystack, const unsigned char * needle) {
    const unsigned char * result = strstr(haystack, needle);
    if ( result != NULL) {
        result += strlen(needle);
    }
    return result;
}


unsigned char * blockToStr(const unsigned char * start, const unsigned char * end) {
    ssize_t length = end - start;
    unsigned char * result = NULL;
    if ( length > 1 ) {
        result = malloc(length + 1);
        if ( result != NULL) {
            memcpy(result, start, length);
            result[ length ] = '\0';
        }
    }
    return result;
}

void setDirectory(const unsigned char * directory, int length) {
    logDebug("directory \'%.*s\'\n", length, directory);
}

void setFilename(const unsigned char * filename, int length) {
    logDebug("filename \'%.*s\'\n", length, filename);
}

#if 0
void processBracketed( const unsigned char *start, const unsigned char *end )
{
    const unsigned char * p;
    const unsigned char * s;
    const unsigned char * lastSep;
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
        const unsigned char * p = start + 1;
        const unsigned char * sep = p;
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
                logDebug("index = %ld\n", index);
                setFilename(p, (end - p));
            } else {
                // all digits after the slash, so assume it's the total
                logDebug("index = %ld, total = %ld\n", index, total);
            }
        } else {
            logDebug("all digits: %ld\n", index);
        }
    } else {
        tHash hash = hashString( start, end - start );
        switch (hash)
        {
        case kHash_FULL:
            logDebug("### FULL\n");
            break;

        case kHash_N3wZ:
            logDebug("### N3wZ\n");
            break;

        case kHash_PRiVATE:
            logDebug("### PRiVATE\n");
            break;

        case kHash_WtFnZb:
            logDebug("### WtFNzB\n");
            break;

        default:
            logDebug("### unhandled: \'%.*s\' %s\n", (int)(end - start), start, describeHash(hash));
            break;
        }
    }
}


bool processToken( tTokenType tokenType, const unsigned char *start, const unsigned char *end )
{
    tHash hash;
    const unsigned char * p;
    const unsigned char * filename = NULL;
    long index;
    long total;

    // trim spaces and dashes from the beginning and end
    while ( start < end && (*start == ' ' || *start == '-') ) { start++; }
    do { --end; } while ( start < end && (*end == ' ' || *end == '-') );
    end++;

    if ( (end - start) > 0 ) {
        logDebug( "%s: \'%.*s\'\n", tokenTypeNames[tokenType], (int)(end - start), start );

        switch ( tokenType )
        {
        case kToken_Unquoted:
            if ( strncmp( start, "yEnc", 4 ) == 0 ) {
                logDebug( "### yEnc\n" );
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
        logDebug( "%s: (empty)\n", tokenTypeNames[tokenType] );
    }
#endif
    return false;
}
#endif

/**
 * find the last occurrence of needle in haystack.
 *
 * similar to strstr(), except it finds the last match, not the first
 * @param haystack the string to search
 * @param needle what substr to look for
 * @return NULL if not found, otherwise a pointer to the last occurrence in haystack of needle.
 */
unsigned char * strrstr(unsigned char * haystack, const unsigned char * needle) {
    unsigned char * result = NULL;
    while ( *haystack != '\0' ) {
        if ( *haystack == *needle ) {
            int i = 1;
            while ( needle[ i ] != '\0' && haystack[ i ] == needle[ i ] ) { i++; }
            if ( needle[ i ] == '\0' ) { result = haystack; }
        }
        haystack++;
    }
    return result;
}

tSignature identifyToken(tHash hash, const byte * str, size_t len) {
    const byte * p;
    unsigned int value;
    unsigned int divisor;
    tSignature result;

    switch ( hash ) {
    case kHash_Empty:
        result = kToken_Empty;
        break;
    case kHash_N3wZ:
        result = kToken_N3wZ;
        break;
    case kHash_newzNZB:
        result = kToken_newzNZB;
        break;
    case kHash_WtFnZb:
        result = kToken_WtFnZb;
        break;

    case kHash_PRiVATE:
        result = kToken_PRiVATE;
        break;
    case kHash_FULL:
        result = kToken_FULL;
        break;
    case kHash_yEnc:
        result = kToken_yEnc;
        break;
    case kHash_Of:
        result = kToken_Of;
        break;

    default:
        result = kToken_Number;
        value = 0;
        divisor = 0;
        for ( int i = 0; i < len; i++ ) {
            if ( isdigit(str[ i ])) {
                value = value * 10 + (str[ i ] - '0');
            } else if ( str[ i ] == '/' ) {
                divisor = value;
                value = 0;
                result = kToken_Fraction;
            } else {
                // result = kToken_Quoted;
                result = kToken_String;
                break;
            }
        }
        break;
    }

    return result;
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

unsigned char * preprocessSubject(const unsigned char * subject) {
    fprintf(stdout, "\ns: %s\n", subject);
    unsigned char * subj = (byte *) strdup(subject);

    /* Trim a yEnc suffix, if present.
     * Trim only at the last one - I've seen cases where another 'yEnc'
     * keyword is embedded in the _middle_ of the subject ?!?! */
    unsigned char * e = strrstr(subj, " yEnc");
    if ( e != NULL) {
        /* back up over trailing separators */
        while ( e > subject && charMap[ *e ].runEndType == kSeparator ) { --e; }
        /* terminate the string */
        e[ 1 ] = '\0';
    }

#if 0
    unsigned char * s = subj;
    unsigned char * d = s;

    unsigned char prevCh = '\0';

    do {
        if ( (prevCh == '[' || prevCh == '"' || prevCh == '-') && *s == ' ' ) {
            /* skip the space, copy the next unsigned char */
            *d++ = *(++s);
        } else if ( prevCh == ' ' && (*s == ']' || *s == '-' || *s == '"' || *s == ' ')) {
            /* overwrite the space with the currecnt unsigned char */
            d[-1] = *s;
        } else {
            /* not a special case, just copy it */
            *d++ = *s;
        }
        prevCh = *s;

    } while ( *s++ != '\0' );

    fprintf( stdout, "d: %s\n", subj );
#endif
    return subj;
}

void processSubject(const unsigned char * subject)
{
    const unsigned char * tokenStart;
    const unsigned char * tokenEnd;
    unsigned int tokenLen;
             int tokenLevel = 0;
    const unsigned char * separatorStart;
    const unsigned char * filename = NULL;

    tHash hash = kHash_Empty;

#ifdef DEBUG
    char debug[4][1024];
    memset(debug, '?', sizeof(debug));
#endif


    unsigned char * subj = preprocessSubject(subject);

    const unsigned char * p = subj;
    tokenStart = p;
    separatorStart = p;

    enum eRunEndType wasEndRun = kNotEnd;

    do {
        enum eRunEndType endRun  = charMap[ *p ].runEndType;

#ifdef DEBUG
        unsigned int i = (unsigned int) (p - subj);
        if ( i >= sizeof(debug[ 0 ])) i = sizeof(debug[ 0 ]) - 1;

        debug[ 0 ][ i ] = *p;
        debug[ 1 ][ i ] = '0' + (endRun);
        debug[ 2 ][ i ] = (endRun != kNotEnd) ? '^' : '_';
        debug[ 3 ][ i ] = (char) ('0' + tokenLevel);
#endif

//        if (tokenLevel == 0) {
            if ( endRun != kNotEnd && wasEndRun == kNotEnd && (p - tokenStart) > 1 ) {
                logDebug("%s: \'%.*s\' hash: 0x%016lx\n", runEndTypeAsString[endRun], (int) (tokenEnd - tokenStart), tokenStart, hash);
                tokenStart = p;
                hash = kHash_Empty;
            }

            if ( (endRun != kSeparator) && (wasEndRun == kSeparator) && (p - separatorStart) > 1 ) {
                /* end of separator run */
                /* emit previous token run */
                logDebug("sep: \'%.*s\' hash: 0x%016lx\n", (int) (p - separatorStart), separatorStart, hash);
                /* reset start of token run */
                hash = kHash_Empty;
            }
//        }

        switch ( endRun ) {

        case kDoubleQuotes:
            if ( tokenLevel == 0) {
                /* start of quoted string */
                tokenStart = p + 1;
                hash = kHash_Empty;
                ++tokenLevel;
            } else {
                /* end of quoted string */
                if ( tokenStart == p ) {
                    logDebug("empty quotes\n");
                } else {
                    logDebug("quoted: \"%.*s\"\n", (int) (tokenEnd - tokenStart), tokenStart);
                }
                --tokenLevel;
            }
            break;

        case kLeftSquareBracket:
            if ( p[ 1 ] == '[' ) {
                /* ignore '[[' */
                p++;
            } else {
                if ( tokenLevel == 0 ) {
                    tokenStart = p + 1;
                    /* skip over any leading separators */
                    while ( charMap[ *tokenStart ].runEndType == kSeparator ) { ++tokenStart; }
                    hash = kHash_Empty;
                }
                ++tokenLevel;
            }
            break;

        case kRightSquareBracket:
            if ( p[ 1 ] == ']' ) {
                p++;
            } else {
                --tokenLevel;
                if ( tokenLevel < 1 ) {
                    tokenLen = tokenEnd - tokenStart;
                    logDebug("token: \'%.*s\'", tokenLen, tokenStart);
                    if ( tokenLen < 12 ) {
                        logDebug(" hash: 0x%016lx", hash);
                    }
                    logDebug("\n");
                }
            }
            break;

        // case kNotEnd:
        case kSeparator:
            if ( tokenLevel == 0 ) {
                if ( wasEndRun != kSeparator ) {
                    /* start of separator run */
                    separatorStart = p;
                    hash = kHash_Empty;
                }
                tokenStart = p + 1;
            } else
        default:
            {
                hash ^= (hash * 47) + *p;
                tokenEnd = p + 1;
            }
            break;
        }

#if 0
        if ( atSeparator ) {
            if ( prevState == false ) {
                // at the token/separator boundary
                separatorStart = p;

                prevHash = hash;
                hash = kHash_Empty;

            }
        } else {
            if ( prevState == true ) {
                // at the separator/token boundary

                for ( int k = 0; separatorHashTable[ k ].hash != 0; k++ ) {
                    if ( separatorHashTable[ k ].hash == hash ) {

#ifdef DEBUG
                        if ( tokenStart == separatorStart ) {
                            debug[ 2 ][ tokenStart - subj ] = '^';
                        } else {
                            debug[ 2 ][ tokenStart - subj ] = '<';
                            debug[ 2 ][ separatorStart - subj ] = '>';
#endif
                            prev2Token = prevToken;
                            prevToken = token;
                            token = identifyToken( prevHash, tokenStart, separatorStart - tokenStart );

                            switch ( token ) {
                            case kToken_Quoted:
                                if ( filename == NULL) {
                                    filename = blockToStr(tokenStart, separatorStart);
                                }
                                break;

                            case kToken_String:
                                if ( filename == NULL && prev2Token == kToken_PRiVATE && prevToken == kToken_WtFnZb ) {
                                    filename = blockToStr(tokenStart, separatorStart);
                                }
                                break;

                            case kToken_Number:
                                if ( prev2Token == kToken_Number && prevToken == kToken_Of ) {
                                    logDebug( "(fraction)");
                                }
                                break;
                            }
#ifdef DEBUG
                            logDebug( "%10s \'%.*s\' (0x%lx)\n",
                                      tokenTypeNames[token],
                                      (int)(separatorStart - tokenStart), tokenStart,
                                      prevHash );
                        }
                        debug[ 4 ][ i ] = (char) ('0' + (tokenStart - separatorStart));

                        logDebug("       sep \'%.*s\' (0x%lx)",
                                 (int) (p - separatorStart), separatorStart, hash);
                        int levelChanges = 0;
#endif

                        for ( int j = 0; j < 2; j++ ) {
                            logDebug(" %s", sepTokenToString[ separatorHashTable[ k ].index[ j ]]);

                            switch ( separatorHashTable[ k ].index[ j ] ) {

                            case kSep_startSq:              // '[', '[ ', ' [', ' [ '
                            case kSep_startQuotes:          // ' "',
                            case kSep_startBracket:         // '(', ' (', '  ('
                                tokenLevel++;
#ifdef DEBUG
                                levelChanges++;
#endif
                                break;

                            case kSep_endSq:                // ']', '] ', ']  '
                            case kSep_endQuotes:            // '" ', '"  '
                            case kSep_endBracket:           // ')', ') '
                                tokenLevel--;
#ifdef DEBUG
                                levelChanges++;
#endif
                                break;

                            case kSep_dash:
                            case kSep_emptyQuotes:          // '""'
                            case kSep_space:                // ' ', '  '
                            case kSep_nop:
                            default:
                                break;
                            }
                        }
#ifdef DEBUG
                        if ( levelChanges > 1 ) {
                            debug[ 3 ][ i ] = 'x';
                        }
                        logDebug(" [%d,%d]\n", tokenLevel, levelChanges);
#endif
                        tokenStart = p;
                        break;
                    }
                }
                prevHash = hash;
                hash = kHash_Empty;
            }
        }

        hash ^= ( hash * 47 ) + *p;
#endif
        wasEndRun = endRun;
    } while ( *p++ != '\0' );

#ifdef DEBUG
    unsigned int i = (unsigned int) (p - subj - 1);
    for ( int j = 0; j < 4; j++ ) {
        debug[ j ][ i ] = '\0';
        logDebug("%d: %s\n", j, debug[ j ]);
    }
#endif

    free(subj);
    free((void *) filename);
}

void processElement(tElement * element) {
#ifdef DEBUG_VERBOSE
    if ( element != NULL && element->elementHash != kHash_Segment ) {
        logDebug( "   >  %s: \'%s\'\n", describeHash(element->elementHash), element->contents );

        for ( tAttribute * attribute = element->attributes;
              attribute != NULL;
              attribute = attribute->next )
        {
            logDebug( "     >  %s: \'%s\'\n", describeHash(attribute->attributeHash ), attribute->value );
        }
    }
#endif
}

int processFile(FILE * file) {
    yxml_t xml;
    yxml_ret_t r = YXML_OK;
    char buffer[4096];
    char value[1024];

    int level = 0;
    yxml_init(&xml, buffer, sizeof(buffer));

    tElement * element = NULL;
    tAttribute * attribute = NULL;
    tElement * newElement;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        r = yxml_parse(&xml, ch);
        switch ( r ) {
        case YXML_ELEMSTART:
#ifdef DEBUG_VERBOSE
            logDebug( "%d  ElemStart %s = 0x%016lx\n", level, xml.elem, hashString( xml.elem ) );
#endif
            if ((newElement = calloc(1, sizeof(tElement))) != NULL) {
                newElement->elementHash = hashString(xml.elem, 0);

                /* push new entry on the element stack */
                newElement->next = element;
                element = newElement;
            }
            level++;
            value[ 0 ] = '\0';
            break;

        case YXML_ATTRSTART:
#ifdef DEBUG_VERBOSE
            logDebug( "   AttrStart %s = 0x%016lx\n", xml.attr, hashString( xml.attr ) );
#endif
            if ((attribute = calloc(1, sizeof(tAttribute))) != NULL) {
                attribute->attributeHash = hashString(xml.attr, 0);
            }
            attribute->next = element->attributes;
            element->attributes = attribute;
            value[ 0 ] = '\0';
            break;

        case YXML_ATTRVAL:
        case YXML_CONTENT:
            strncat(value, xml.data, sizeof(value) - 2);
            break;

        case YXML_ATTREND:
#ifdef DEBUG_VERBOSE
            logDebug( "   AttrEnd %s \'%s\'\n", xml.attr, value );
#endif
            attribute->value = strdup(value);
            if ( element->elementHash == kHash_File && attribute->attributeHash == kHash_Subject ) {
                processSubject(attribute->value);
            }

            value[ 0 ] = '\0';
            break;

        case YXML_ELEMEND:
            --level;
#ifdef DEBUG_VERBOSE
            logDebug( "%d  ElemEnd %s \'%s\'\n", level, xml.elem, value );
#endif
            if ( element != NULL) {
                void * temp;

                trimstr(value);
                if ( strlen(value) > 0 ) {
                    element->contents = strdup(value);
                    value[ 0 ] = '\0';
                }

                processElement(element);

                // release attributes assigned to the element at the top of the element stack
                attribute = element->attributes;
                while ( attribute != NULL) {
                    temp = attribute;
                    attribute = attribute->next;
                    free(temp);
                }
                // 'pop' the top of the element stack, and release its memory
                temp = element;
                element = element->next;
                free(temp);
            }
            break;

        default:
            if ( r < 0 ) {
                fprintf(stderr, "xml parser error %d\n", r);
                exit(r);
            }
            break;
        }
    }

    r = yxml_eof(&xml);
    if ( r != YXML_OK ) {
        fprintf(stderr, "xml error %d at end of file", r);
    }

    return r;
}

int main(int argc, const char * argv[]) {
    const char * myName = strrchr(argv[ 0 ], '/');
    if ( myName++ == NULL) {
        myName = argv[ 0 ];
    }

    if ( argc < 2 ) {
        processFile(stdin);
    } else {
        for ( int i = 1; i < argc; ++i ) {
            FILE * file = fopen(argv[ i ], "r");
            if ( file == NULL) {
                fprintf(stderr,
                        "### %s: error: unable to open \'%s\' (%d: %s)\n",
                        myName, argv[ i ], errno, strerror(errno));
                exit(-errno);
            } else {
                processFile(file);
                fclose(file);
            }
        }
    }

    return 0;
}
