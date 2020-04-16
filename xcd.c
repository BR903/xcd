/*
 * xcd.c: A colorizing hexdump utility.
 *
 * Copyright (C) 2018 Brian Raiter <breadbox@muppetlabs.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <term.h>
#include <getopt.h>

typedef unsigned char byte;

/* Online help.
 */
static char const *yowzitch =
    "Usage: xcd [OPTIONS] [FILENAME]...\n"
    "Output the contents of FILENAME as a hex dump (displaying octets as\n"
    "hexadecimal values, and characters when appropriate), using contrasting\n"
    "colors to help bring out patterns. With multiple arguments, the files'\n"
    "contents are concatenated together. With no arguments, or when FILENAME\n"
    "is -, read from standard input.\n"
    "\n"
    "  -c, --count=N         Display N bytes per line [default=16]\n"
    "  -g, --group=N         Display N bytes per groups [default=2]\n"
    "  -s, --start=N         Start N bytes after start of input\n"
    "  -l, --limit=N         Stop after N bytes of input\n"
    "  -a, --autoskip        Omit lines of zero bytes with a single \"*\"\n"
    "  -N, --no-color        Suppress color output\n"
    "  -R, --raw             Dump colorized bytes without the hex display\n"
    "  -A, --ascii           Don't use Unicode characters in text column\n"
    "      --help            Display this help and exit\n"
    "      --version         Display version information and exit\n";

/* Version information.
 */
static char const *vourzhon =
    "xcd: v1.2\n"
    "Copyright (C) 2018 by Brian Raiter <breadbox@muppetlabs.com>\n"
    "This is free software; you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n";

/* The color index palette. This is the list of color indexes for the
 * 256-color xterm, sorted to maximize visual contrast. Colors should
 * be assigned preferentially from the start of this list.
 *
 * To create this index, high-contrast color sets created by design
 * researchers were consulted. References to their papers can be found
 * at <https://graphicdesign.stackexchange.com/a/3815>. The Kenneth
 * Kelly palette was the main source, with some modifications (such as
 * reserving black and other colors often used for normal text). The
 * resulting list of colors was then matched with entries the xterm
 * palette, as close as possible. Finally, the remaining colors in the
 * xterm palette were added, sorted by their color-space distance from
 * each other, but skipping over ones that were too dark to read on a
 * black background. Finally, the last two colors are repeated to fill
 * out the palette to 256 entries.
 */
static byte const colorset[] = {
      8,  /* #808080 */
     11,  /* #FFFF00 */
     53,  /* #5F005F */
    202,  /* #FF5F00 */
     87,  /* #5FFFFF */
      9,  /* #FF0000 */
     41,  /* #00D75F */
    217,  /* #FFAFAF */
     32,  /* #0087D7 */
    222,  /* #FFD787 */
     57,  /* #5F00FF */
    214,  /* #FFAF00 */
    126,  /* #AF0087 */
    191,  /* #D7FF5F */
     88,  /* #870000 */
    148,  /* #AFD700 */
     94,  /* #875F00 */
    219,  /* #FFAFFF */
     22,  /* #005F00 */
    228,  /* #FFFF87 */
    121,  /* #87FFAF */
      4,  /* #000080 */
      3,  /* #808000 */
     23,  /* #005F5F */
     30,  /* #008787 */
    179,  /* #D7AF5F */
     14,  /* #00FFFF */
     13,  /* #FF00FF */
    195,  /* #D7FFFF */
     12,  /* #0000FF */
    225,  /* #FFD7FF */
    123,  /* #87FFFF */
    230,  /* #FFFFD7 */
     27,  /* #005FFF */
    159,  /* #AFFFFF */
     10,  /* #00FF00 */
    207,  /* #FF5FFF */
    165,  /* #D700FF */
     50,  /* #00FFD7 */
    227,  /* #FFFF5F */
    235,  /* #262626 */
    200,  /* #FF00D7 */
     45,  /* #00D7FF */
     82,  /* #5FFF00 */
    213,  /* #FF87FF */
    197,  /* #FF005F */
     47,  /* #00FF5F */
    255,  /* #EEEEEE */
     20,  /* #0000D7 */
    190,  /* #D7FF00 */
     93,  /* #8700FF */
    229,  /* #FFFFAF */
    236,  /* #303030 */
     33,  /* #0087FF */
    220,  /* #FFD700 */
    129,  /* #AF00FF */
     49,  /* #00FFAF */
    160,  /* #D70000 */
     39,  /* #00AFFF */
    198,  /* #FF0087 */
    118,  /* #87FF00 */
    199,  /* #FF00AF */
     48,  /* #00FF87 */
    208,  /* #FF8700 */
     63,  /* #5F5FFF */
    154,  /* #AFFF00 */
     81,  /* #5FD7FF */
     52,  /* #5F0000 */
    171,  /* #D75FFF */
    194,  /* #D7FFD7 */
     17,  /* #00005F */
    224,  /* #FFD7D7 */
     40,  /* #00D700 */
    206,  /* #FF5FD7 */
     86,  /* #5FFFD7 */
    237,  /* #3A3A3A */
    189,  /* #D7D7FF */
    203,  /* #FF5F5F */
     83,  /* #5FFF5F */
     19,  /* #0000AF */
    254,  /* #E4E4E4 */
      1,  /* #800000 */
    221,  /* #FFD75F */
    177,  /* #D787FF */
      2,  /* #008000 */
    117,  /* #87D7FF */
     18,  /* #000087 */
    158,  /* #AFFFD7 */
    212,  /* #FF87D7 */
    124,  /* #AF0000 */
    183,  /* #D7AFFF */
     28,  /* #008700 */
    122,  /* #87FFD7 */
    204,  /* #FF5F87 */
     34,  /* #00AF00 */
    153,  /* #AFD7FF */
    193,  /* #D7FFAF */
     69,  /* #5F87FF */
    205,  /* #FF5FAF */
     84,  /* #5FFF87 */
    238,  /* #444444 */
    218,  /* #FFAFD7 */
    192,  /* #D7FF87 */
     99,  /* #875FFF */
    119,  /* #87FF5F */
    135,  /* #AF5FFF */
    209,  /* #FF875F */
     75,  /* #5FAFFF */
    223,  /* #FFD7AF */
     85,  /* #5FFFAF */
    215,  /* #FFAF5F */
     56,  /* #5F00D7 */
    155,  /* #AFFF5F */
    164,  /* #D700D7 */
     58,  /* #5F5F00 */
     44,  /* #00D7D7 */
    161,  /* #D7005F */
    184,  /* #D7D700 */
     26,  /* #005FD7 */
     76,  /* #5FD700 */
    105,  /* #8787FF */
    166,  /* #D75F00 */
    120,  /* #87FF87 */
    141,  /* #AF87FF */
    210,  /* #FF8787 */
    239,  /* #4E4E4E */
    111,  /* #87AFFF */
    156,  /* #AFFF87 */
    211,  /* #FF87AF */
    147,  /* #AFAFFF */
    216,  /* #FFAF87 */
    157,  /* #AFFFAF */
     92,  /* #8700D7 */
     42,  /* #00D787 */
    162,  /* #D70087 */
     38,  /* #00AFD7 */
    112,  /* #87D700 */
    163,  /* #D700AF */
     43,  /* #00D7AF */
    172,  /* #D78700 */
    128,  /* #AF00D7 */
     29,  /* #00875F */
    253,  /* #DADADA */
     54,  /* #5F0087 */
    178,  /* #D7AF00 */
     24,  /* #005F87 */
     55,  /* #5F00AF */
     64,  /* #5F8700 */
    188,  /* #D7D7D7 */
     89,  /* #87005F */
     35,  /* #00AF5F */
     25,  /* #005FAF */
    130,  /* #AF5F00 */
     80,  /* #5FD7D7 */
    125,  /* #AF005F */
     70,  /* #5FAF00 */
    170,  /* #D75FD7 */
    185,  /* #D7D75F */
    240,  /* #585858 */
    252,  /* #D0D0D0 */
     62,  /* #5F5FD7 */
     77,  /* #5FD75F */
      5,  /* #800080 */
    167,  /* #D75F5F */
    152,  /* #AFD7D7 */
      6,  /* #008080 */
    182,  /* #D7AFD7 */
     37,  /* #00AFAF */
    187,  /* #D7D7AF */
     91,  /* #8700AF */
    142,  /* #AFAF00 */
    116,  /* #87D7D7 */
    136,  /* #AF8700 */
    176,  /* #D787D7 */
     31,  /* #0087AF */
    186,  /* #D7D787 */
     90,  /* #870087 */
    106,  /* #87AF00 */
    127,  /* #AF00AF */
     36,  /* #00AF87 */
    100,  /* #878700 */
    251,  /* #C6C6C6 */
     59,  /* #5F5F5F */
     74,  /* #5FAFD7 */
    134,  /* #AF5FD7 */
     79,  /* #5FD7AF */
    149,  /* #AFD75F */
    169,  /* #D75FAF */
    241,  /* #626262 */
     68,  /* #5F87D7 */
    113,  /* #87D75F */
    168,  /* #D75F87 */
     78,  /* #5FD787 */
     98,  /* #875FD7 */
    173,  /* #D7875F */
      7,  /* #C0C0C0 */
    242,  /* #6C6C6C */
    146,  /* #AFAFD7 */
     61,  /* #5F5FAF */
    151,  /* #AFD7AF */
     71,  /* #5FAF5F */
    131,  /* #AF5F5F */
    181,  /* #D7AFAF */
     60,  /* #5F5F87 */
    110,  /* #87AFD7 */
    150,  /* #AFD787 */
    175,  /* #D787AF */
     65,  /* #5F875F */
    115,  /* #87D7AF */
    140,  /* #AF87D7 */
    180,  /* #D7AF87 */
     95,  /* #875F5F */
    104,  /* #8787D7 */
    114,  /* #87D787 */
    174,  /* #D78787 */
    250,  /* #BCBCBC */
    243,  /* #767676 */
     73,  /* #5FAFAF */
    133,  /* #AF5FAF */
    143,  /* #AFAF5F */
     67,  /* #5F87AF */
    107,  /* #87AF5F */
    132,  /* #AF5F87 */
     72,  /* #5FAF87 */
     97,  /* #875FAF */
    137,  /* #AF875F */
     66,  /* #5F8787 */
     96,  /* #875F87 */
    101,  /* #87875F */
    249,  /* #B2B2B2 */
    145,  /* #AFAFAF */
    248,  /* #A8A8A8 */
    109,  /* #87AFAF */
    139,  /* #AF87AF */
    144,  /* #AFAF87 */
    247,  /* #9E9E9E */
    103,  /* #8787AF */
    108,  /* #87AF87 */
    138,  /* #AF8787 */
    246,  /* #949494 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
    245,  /* #8A8A8A */
    102,  /* #878787 */
};

/* The program's input file state and user-controlled settings.
 */
typedef struct state {
    int startoffset;    /* skip over this many chars of input at start */
    int maxinputlen;    /* stop after this many chars of input */
    char **filenames;   /* NULL-terminated list of input filenames */
    FILE *currentfile;  /* handle to the currently open input file */
} state;

/* True if the program should skip repeated lines of zero bytes.
 */
static int autoskip = 0;

/* Number of bytes to display per line of dump output. (The default
 * value is 16, which produces output that fits comfortably on an
 * 80-column display.)
 */
static int linesize = 16;

/* Number of bytes to group together in output.
 */
static int groupsize = 2;

/* The program's (eventual) exit code.
 */
static int exitcode = 0;

/* True if the program should display hexadecimal values.
 */
static int hexoutput = 1;

/* True if the program should add colors to the output.
 */
static int colorize = 1;

/* True if the program should use extended Unicode glyphs to represent
 * non-graphical and non-ASCII characters.
 */
static int useunicode = 1;

/* The width of the output containing the hex byte values.
 */
static int hexwidth;

/* Terminal control sequence strings. setaf is used to change the
 * color of the following characters, and sgr0 is used to reset
 * characters attributes to the default state.
 */
static char const *setaf;
static char const *sgr0;

/* The palette of colors to use for each byte value. Initially each
 * entry is set to zero, meaning unassigned.
 */
static byte palette[256];

/* The index of the first color in colorset that has yet to be
 * assigned to a byte value.
 */
static int nextcolorfromset = 0;

/*
 * Basic functions.
 */

/* Display a formatted message on standard error and exit.
 */
void die(char const *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}

/* Display an error message for the current file and set the exit code.
 */
static void fail(state *s)
{
    perror(s->filenames && *s->filenames ? *s->filenames : "xcd");
    exitcode = EXIT_FAILURE;
}

/* Read a small non-negative integer from a string. Exit with a
 * simple error message if the string is not a valid number, or if
 * the number hits a given upper limit.
 */
static int getn(char const *str, char const *name, int maxval)
{
    char *p;
    long n;

    if (!str || !*str)
        die("missing argument for %s", name);
    n = strtol(str, &p, 0);
    if (*p != '\0' || p == str || (n == LONG_MAX && errno == ERANGE)
                   || n < 0 || n > INT_MAX)
        die("invalid argument '%s' for %s", str, name);
    if (maxval && n > maxval)
        die("value for %s too large (maximum %d)", name, maxval);
    return n;
}

/*
 * File I/O.
 */

/* Prepare the current input file, if necessary. (Does nothing if the
 * current input file is already open and is not at the end.) Any
 * errors that occur when opening a file are reported to stderr before
 * moving on to the next input file. The return value is zero if no
 * more input files are available.
 */
static int inputinit(state *s)
{
    if (!s->currentfile) {
        if (!*s->filenames)
            return 0;
        if (!strcmp(*s->filenames, "-")) {
            s->currentfile = stdin;
            *s->filenames = "stdin";
        } else {
            s->currentfile = fopen(*s->filenames, "r");
        }
        if (!s->currentfile) {
            fail(s);
            ++s->filenames;
            return inputinit(s);
        }
    }
    return 1;
}

/* Close the current input file, reporting errors if any to stderr.
 */
static void inputupdate(state *s)
{
    if (ferror(s->currentfile)) {
        fail(s);
        fclose(s->currentfile);
    } else {
        if (s->currentfile != stdin)
            if (fclose(s->currentfile))
                fail(s);
    }
    s->currentfile = 0;
    ++s->filenames;
}

/* Get the next byte from the current file. If there are no more byte
 * in the current file, open the next file in the list of filenames
 * and read from that. If there are no more filenames, return EOF.
 */
static int nextbyte(state *s)
{
    int ch;

    for (;;) {
        if (!inputinit(s))
            return EOF;
        ch = fgetc(s->currentfile);
        if (ch != EOF)
            return ch;
        inputupdate(s);
    }
}

/* Return a string representing the given character. Latin-1 and
 * Unicode control pictures are used to represent non-ASCII and
 * control characters.
 */
static char const *getbyterepresentation(byte ch)
{
    static char buf[4];

    if (useunicode) {
        if (ch < 32) {
            buf[0] = 0xE2;
            buf[1] = 0x90;
            buf[2] = 0x80 | ch;
            buf[3] = '\0';
        } else if (ch == 32) {
            buf[0] = 0xE2;
            buf[1] = 0x90;
            buf[2] = 0xA0;
            buf[3] = '\0';
        } else if (ch < 127) {
            buf[0] = ch;
            buf[1] = '\0';
        } else if (ch == 127) {
            buf[0] = 0xE2;
            buf[1] = 0x90;
            buf[2] = 0xA1;
            buf[3] = '\0';
        } else if (ch < 160) {
            buf[0] = 0xE2;
            buf[1] = 0x90;
            buf[2] = 0xA6;
            buf[3] = '\0';
        } else if (ch == 160) {
            buf[0] = 0xE2;
            buf[1] = 0x90;
            buf[2] = 0xA3;
            buf[3] = '\0';
        } else {
            buf[0] = 0xC0 | (ch >> 6);
            buf[1] = 0x80 | (ch & 0x3F);
            buf[2] = '\0';
        }
    } else {
        buf[0] = isprint(ch) ? ch : '.';
        buf[1] = '\0';
    }
    return buf;
}

/*
 * Terminal handling functions.
 */

/* Look up the terminal in the terminfo database and initialize it to
 * do color output.
 */
static void initoutput(void)
{
    char *termname, *seq;
    int err;

    if (!colorize)
        return;
    termname = getenv("TERM");
    if (setupterm(termname, 1, &err) != 0) {
        if (err < 0)
            fprintf(stderr, "error: cannot find terminfo database.\n");
        else if (err == 0 || !termname || !*termname)
            fprintf(stderr, "error: cannot identify terminal type.\n");
        else
            fprintf(stderr, "error: terminal \"%s\" lacks color; use xxd.\n",
                    termname);
        exit(EXIT_FAILURE);
    }
    sgr0 = tigetstr("sgr0");
    setaf = tigetstr("setaf");
    if (!sgr0 || !setaf) {
        fprintf(stderr, "error: terminal \"%s\" lacks color; use --no-color"
                        " (or xxd(1)).\n",
                termname);
        exit(EXIT_FAILURE);
    }
    if (tigetnum("colors") < 256) {
        fprintf(stderr, "error: colorizing requires 256 colors,"
                        " but only %d are available.\n",
                tigetnum("colors"));
        exit(EXIT_FAILURE);
    }
    seq = tigetstr("is1");
    if (seq)
        fputs(seq, stdout);
    seq = tigetstr("is2");
    if (seq)
        fputs(seq, stdout);
    seq = tigetstr("is3");
    if (seq)
        fputs(seq, stdout);

    palette[0] = colorset[0];
    nextcolorfromset = 1;
}

/*
 * Three different dump format functions.
 */

/* Output colorized bytes directly.
 */
static void renderbytescolored(byte const *buf, int count)
{
    int ch, i;

    for (i = 0 ; i < count ; ++i) {
        ch = buf[i];
        if (!isgraph(ch)) {
            putchar(ch);
            continue;
        }
        if (!palette[ch])
            palette[ch] = colorset[nextcolorfromset++];
        printf("%s%s", tiparm(setaf, palette[ch]), getbyterepresentation(ch));
    }
    printf("%s", sgr0);
}

/* Output one line of data as a hexdump, containing up to linesize
 * bytes. pos supplies the current file position.
 */
static void renderlineuncolored(byte const *buf, int count, int pos)
{
    int i, x;

    printf("%08X:", pos);
    x = hexwidth - 2 * count;
    for (i = 0 ; i < count ; ++i) {
        if (i % groupsize == 0) {
            putchar(' ');
            --x;
        }
        printf("%02X", buf[i]);
    }
    printf("%*s  ", x, "");
    for (i = 0 ; i < count ; ++i)
        fputs(getbyterepresentation(buf[i]), stdout);
    putchar('\n');
}

/* Output one line of data as a hexdump, containing up to linesize
 * bytes. pos supplies the current file position.
 */
static void renderlinecolored(byte const *buf, int count, int pos)
{
    int ch, i, x;

    printf("%s%08X:", sgr0, pos);
    x = hexwidth - 2 * count;
    for (i = 0 ; i < count ; ++i) {
        if (i % groupsize == 0) {
            putchar(' ');
            --x;
        }
        ch = buf[i];
        if (!palette[ch])
            palette[ch] = colorset[nextcolorfromset++];
        printf("%s%02X", tiparm(setaf, palette[ch]), ch);
    }
    printf("%s%*s  ", sgr0, x, "");
    for (i = 0 ; i < count ; ++i) {
        ch = buf[i];
        if (!palette[ch])
            palette[ch] = colorset[nextcolorfromset++];
        printf("%s%s", tiparm(setaf, palette[ch]), getbyterepresentation(ch));
    }
    printf("%s\n", sgr0);
}

/*
 * The line output functions.
 */

/* Display a single line of a hexdump appropriate in the requested
 * format.
 */
static void dumpline(byte const *buf, int count, int pos)
{
    if (count) {
        if (!hexoutput)
            renderbytescolored(buf, count);
        else if (colorize)
            renderlinecolored(buf, count, pos);
        else
            renderlineuncolored(buf, count, pos);
    }
}

/* Display some hexdump lines consisting entirely of zero bytes. If
 * count is three or more, all but the first are elided.
 */
static void dumpzerolines(int count, int pos)
{
    static byte zeroline[256];
    int i;

    if (count > 2) {
        dumpline(zeroline, linesize, pos);
        printf("*\n");
    } else {
        for (i = 0 ; i < count ; ++i)
            dumpline(zeroline, linesize, pos + i * linesize);
    }
}

/* Display hexdump lines from the given filenames until there's no
 * more input.
 */
static void dumpfiles(state *s, int pos)
{
    byte    line[256];
    int     ch = 0, n;

    while (ch != EOF && s->maxinputlen > 0) {
        for (n = 0 ; n < linesize && s->maxinputlen > 0 ; ++n) {
            ch = nextbyte(s);
            if (ch == EOF)
                break;
            line[n] = ch;
            --s->maxinputlen;
        }
        if (n == 0)
            break;
        dumpline(line, n, pos);
        pos += n;
    }
}

/* Display hexdump lines from the given filenames until there's no
 * more input. Lines are checked for the presence of nonzero bytes,
 * and lines that are all zeroes are deferred until a nonzero byte is
 * found. At that point, if three or more lines of zeroes were found,
 * all but the first are omitted. If the end of input is reached while
 * searching for a nonzero byte, then all but the first and last line
 * are omitted.
 */
static void dumpfileswithautoskip(state *s, int pos)
{
    byte    line[256];
    int     linesheld = 0, lastheldsize = 0, holdpos = 0;
    int     nonzero, ch = 0, n;

    while (ch != EOF && s->maxinputlen > 0) {
        nonzero = 0;
        for (n = 0 ; n < linesize && s->maxinputlen > 0 ; ++n) {
            ch = nextbyte(s);
            if (ch == EOF)
                break;
            line[n] = ch;
            nonzero |= ch;
            --s->maxinputlen;
        }
        if (n == 0)
            break;
        if (nonzero) {
            if (linesheld) {
                dumpzerolines(linesheld, holdpos);
                linesheld = 0;
            }
            dumpline(line, n, pos);
        } else {
            if (linesheld == 0)
                holdpos = pos;
            ++linesheld;
            lastheldsize = n;
        }
        pos += n;
    }

    if (linesheld) {
        dumpzerolines(linesheld - 1, holdpos);
        dumpline(line, lastheldsize, pos - lastheldsize);
    }
}

/* Move the input stream to the starting point and then 
 */
static void dump(state *s)
{
    int pos;

    for (pos = 0 ; pos < s->startoffset ; ++pos)
        if (nextbyte(s) == EOF)
	    return;

    if (autoskip)
        dumpfileswithautoskip(s, pos);
    else
        dumpfiles(s, pos);
}

/*
 * The top-level functions.
 */

/* Parse the command-line arguments and initialize the given state
 * appropriately, as well as default values. Invalid arguments (or
 * invalid combinations of arguments) will cause the program to exit.
 */
static void parsecommandline(int argc, char *argv[], state *s)
{
    static char *defaultargs[] = { "-", NULL };
    static char const *optstring = "Aac:g:l:NRs:";
    static struct option options[] = {
        { "count", required_argument, NULL, 'c' },
        { "group", required_argument, NULL, 'g' },
        { "limit", required_argument, NULL, 'l' },
        { "start", required_argument, NULL, 's' },
        { "autoskip", no_argument, NULL, 'a' },
        { "no-color", no_argument, NULL, 'N' },
        { "raw", no_argument, NULL, 'R' },
        { "ascii", no_argument, NULL, 'A' },
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'v' },
        { 0, 0, 0, 0 }
    };

    int ch;

    s->startoffset = 0;
    s->maxinputlen = INT_MAX;
    s->filenames = defaultargs;
    s->currentfile = NULL;

    while ((ch = getopt_long(argc, argv, optstring, options, NULL)) != EOF) {
        switch (ch) {
          case 'l':     s->maxinputlen = getn(optarg, "limit", 0);  break;
          case 's':     s->startoffset = getn(optarg, "start", 0);  break;
          case 'c':     linesize = getn(optarg, "count", 255);      break;
          case 'g':     groupsize = getn(optarg, "group", 0);       break;
          case 'a':     autoskip = 1;                               break;
          case 'N':     colorize = 0;                               break;
          case 'R':     hexoutput = 0;                              break;
          case 'A':     useunicode = 0;                             break;
          case 'h':     fputs(yowzitch, stdout);                    exit(0);
          case 'v':     fputs(vourzhon, stdout);                    exit(0);
          default:      die("Try --help for more information.");
        }
    }
    if (!hexoutput) {
        autoskip = 0;
        if (!colorize)
            die("cannot use both --raw and --no-color.");
    }

    if (linesize == 0)
        linesize = 16;
    if (groupsize == 0)
        groupsize = linesize;
    hexwidth = 2 * linesize + (linesize + groupsize - 1) / groupsize;

    if (optind < argc)
        s->filenames = argv + optind;
}

/* Main itself.
 */
int main(int argc, char *argv[])
{
    state s;

    parsecommandline(argc, argv, &s);
    initoutput();
    dump(&s);
    return exitcode;
}
