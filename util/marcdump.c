/* This file is part of the YAZ toolkit.
 * Copyright (C) 1995-2013 Index Data
 * See the file LICENSE for details.
 */

#define _FILE_OFFSET_BITS 64

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if YAZ_HAVE_XML2
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/* Libxml2 version < 2.6.15. xmlreader not reliable/present */
#if LIBXML_VERSION < 20615
#define USE_XMLREADER 0
#else
#define USE_XMLREADER 1
#endif

#if USE_XMLREADER
#include <libxml/xmlreader.h>
#endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif
#if HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include <yaz/marcdisp.h>
#include <yaz/yaz-util.h>
#include <yaz/xmalloc.h>
#include <yaz/options.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif


static char *prog;

static int no_errors = 0;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-i format] [-o format] [-f from] [-t to] "
            "[-l pos=value] [-c cfile] [-s prefix] [-C size] [-n] "
            "[-p] [-v] [-V] file...\n",
            prog);
}

static void show_version(void)
{
    char vstr[20], sha1_str[41];

    yaz_version(vstr, sha1_str);
    printf("YAZ version: %s %s\n", YAZ_VERSION, YAZ_VERSION_SHA1);
    if (strcmp(sha1_str, YAZ_VERSION_SHA1))
        printf("YAZ DLL/SO: %s %s\n", vstr, sha1_str);
    exit(0);
}

static int getbyte_stream(void *client_data)
{
    FILE *f = (FILE*) client_data;

    int c = fgetc(f);
    if (c == EOF)
        return 0;
    return c;
}

static void ungetbyte_stream(int c, void *client_data)
{
    FILE *f = (FILE*) client_data;

    if (c == 0)
        c = EOF;
    ungetc(c, f);
}

static void marcdump_read_line(yaz_marc_t mt, const char *fname)
{
    FILE *inf = fopen(fname, "rb");
    if (!inf)
    {
        fprintf(stderr, "%s: cannot open %s:%s\n",
                prog, fname, strerror(errno));
        exit(1);
    }

    while (yaz_marc_read_line(mt, getbyte_stream,
                              ungetbyte_stream, inf) == 0)
    {
        WRBUF wrbuf = wrbuf_alloc();
        yaz_marc_write_mode(mt, wrbuf);
        fputs(wrbuf_cstr(wrbuf), stdout);
        wrbuf_destroy(wrbuf);
    }
    fclose(inf);
}

#if YAZ_HAVE_XML2
static void marcdump_read_xml(yaz_marc_t mt, const char *fname)
{
    WRBUF wrbuf = wrbuf_alloc();
#if USE_XMLREADER
    xmlTextReaderPtr reader = xmlReaderForFile(fname, 0 /* encoding */,
                                               0 /* options */);

    if (reader)
    {
        int ret;
        while ((ret = xmlTextReaderRead(reader)) == 1)
        {
            int type = xmlTextReaderNodeType(reader);
            if (type == XML_READER_TYPE_ELEMENT)
            {
                const char *name = (const char *)
                    xmlTextReaderLocalName(reader);
                if (!strcmp(name, "record") || !strcmp(name, "r"))
                {
                    xmlNodePtr ptr = xmlTextReaderExpand(reader);

                    int r = yaz_marc_read_xml(mt, ptr);
                    if (r)
                    {
                        no_errors++;
                        fprintf(stderr, "yaz_marc_read_xml failed\n");
                    }
                    else
                    {
                        int write_rc = yaz_marc_write_mode(mt, wrbuf);
                        if (write_rc)
                        {
                            yaz_log(YLOG_WARN, "yaz_marc_write_mode: "
                                    "write error: %d", write_rc);
                            no_errors++;
                        }
                        fputs(wrbuf_cstr(wrbuf), stdout);
                        wrbuf_rewind(wrbuf);
                    }
                }
            }
        }
    }
#else
    xmlDocPtr doc = xmlParseFile(fname);
    if (doc)
    {
        xmlNodePtr ptr = xmlDocGetRootElement(doc);
        for (; ptr; ptr = ptr->next)
        {
            if (ptr->type == XML_ELEMENT_NODE)
            {
                if (!strcmp((const char *) ptr->name, "collection"))
                {
                    ptr = ptr->children;
                    continue;
                }
                if (!strcmp((const char *) ptr->name, "record") ||
                    !strcmp((const char *) ptr->name, "r"))
                {
                    int r = yaz_marc_read_xml(mt, ptr);
                    if (r)
                    {
                        no_errors++;
                        fprintf(stderr, "yaz_marc_read_xml failed\n");
                    }
                    else
                    {
                        yaz_marc_write_mode(mt, wrbuf);

                        fputs(wrbuf_cstr(wrbuf), stdout);
                        wrbuf_rewind(wrbuf);
                    }
                }
            }
        }
        xmlFreeDoc(doc);
    }
#endif
    fputs(wrbuf_cstr(wrbuf), stdout);
    wrbuf_destroy(wrbuf);
}
#endif

static void dump(const char *fname, const char *from, const char *to,
                 int input_format, int output_format,
                 int write_using_libxml2,
                 int print_offset, const char *split_fname, int split_chunk,
                 int verbose, FILE *cfile, const char *leader_spec)
{
    yaz_marc_t mt = yaz_marc_create();
    yaz_iconv_t cd = 0;

    if (yaz_marc_leader_spec(mt, leader_spec))
    {
        fprintf(stderr, "bad leader spec: %s\n", leader_spec);
        yaz_marc_destroy(mt);
        exit(2);
    }
    if (from && to)
    {
        cd = yaz_iconv_open(to, from);
        if (!cd)
        {
            fprintf(stderr, "conversion from %s to %s "
                    "unsupported\n", from, to);
            yaz_marc_destroy(mt);
            exit(2);
        }
        yaz_marc_iconv(mt, cd);
    }
    yaz_marc_enable_collection(mt);
    yaz_marc_xml(mt, output_format);
    yaz_marc_write_using_libxml2(mt, write_using_libxml2);
    yaz_marc_debug(mt, verbose);

    if (input_format == YAZ_MARC_MARCXML || input_format == YAZ_MARC_TURBOMARC || input_format == YAZ_MARC_XCHANGE)
    {
#if YAZ_HAVE_XML2
        marcdump_read_xml(mt, fname);
#endif
    }
    else if (input_format == YAZ_MARC_LINE)
    {
        marcdump_read_line(mt, fname);
    }
    else if (input_format == YAZ_MARC_ISO2709)
    {
        FILE *inf = fopen(fname, "rb");
        int num = 1;
        int marc_no = 0;
        int split_file_no = -1;
        if (!inf)
        {
            fprintf(stderr, "%s: cannot open %s:%s\n",
                    prog, fname, strerror(errno));
            exit(1);
        }
        if (cfile)
            fprintf(cfile, "char *marc_records[] = {\n");
        for(;; marc_no++)
        {
            const char *result = 0;
            size_t len;
            size_t rlen;
            size_t len_result;
            size_t r;
            char buf[100001];

            r = fread(buf, 1, 5, inf);
            if (r < 5)
            {
                if (r == 0) /* normal EOF, all good */
                    break;
                if (print_offset && verbose)
                {
                    printf("<!-- Extra %ld bytes at end of file -->\n",
                           (long) r);
                }
                break;
            }
            while (*buf < '0' || *buf > '9')
            {
                int i;
                long off = ftell(inf) - 5;
                printf("<!-- Skipping bad byte %d (0x%02X) at offset "
                       "%ld (0x%lx) -->\n",
                       *buf & 0xff, *buf & 0xff,
                       off, off);
                for (i = 0; i<4; i++)
                    buf[i] = buf[i+1];
                r = fread(buf+4, 1, 1, inf);
                no_errors++;
                if (r < 1)
                    break;
            }
            if (r < 1)
            {
                if (verbose || print_offset)
                    printf("<!-- End of file with data -->\n");
                break;
            }
            if (print_offset)
            {
                long off = ftell(inf) - 5;
                printf("<!-- Record %d offset %ld (0x%lx) -->\n",
                       num, off, off);
            }
            len = atoi_n(buf, 5);
            if (len < 25 || len > 100000)
            {
                long off = ftell(inf) - 5;
                printf("<!-- Bad Length %ld read at offset %ld (%lx) -->\n",
                       (long)len, (long) off, (long) off);
                no_errors++;
                break;
            }
            rlen = len - 5;
            r = fread(buf + 5, 1, rlen, inf);
            if (r < rlen)
            {
                long off = ftell(inf);
                printf("<!-- Premature EOF at offset %ld (%lx) -->\n",
                       (long) off, (long) off);
                no_errors++;
                break;
            }
            while (buf[len-1] != ISO2709_RS)
            {
                if (len > sizeof(buf)-2)
                {
                    r = 0;
                    break;
                }
                r = fread(buf + len, 1, 1, inf);
                if (r != 1)
                    break;
                len++;
            }
            if (r < 1)
            {
                printf("<!-- EOF while searching for RS -->\n");
                no_errors++;
                break;
            }
            if (split_fname)
            {
                char fname[256];
                const char *mode = 0;
                FILE *sf;
                if ((marc_no % split_chunk) == 0)
                {
                    mode = "wb";
                    split_file_no++;
                }
                else
                    mode = "ab";
                sprintf(fname, "%.200s%07d", split_fname, split_file_no);
                sf = fopen(fname, mode);
                if (!sf)
                {
                    fprintf(stderr, "Could not open %s\n", fname);
                    split_fname = 0;
                }
                else
                {
                    if (fwrite(buf, 1, len, sf) != len)
                    {
                        fprintf(stderr, "Could write content to %s\n",
                                fname);
                        split_fname = 0;
                        no_errors++;
                    }
                    fclose(sf);
                }
            }
            len_result = rlen;
            r = yaz_marc_decode_buf(mt, buf, -1, &result, &len_result);
            if (r == -1)
                no_errors++;
            if (r > 0 && result && len_result)
            {
                if (fwrite(result, len_result, 1, stdout) != 1)
                {
                    fprintf(stderr, "Write to stdout failed\n");
                    no_errors++;
                    break;
                }
            }
            if (r > 0 && cfile)
            {
                char *p = buf;
                size_t i;
                if (marc_no)
                    fprintf(cfile, ",");
                fprintf(cfile, "\n");
                for (i = 0; i < r; i++)
                {
                    if ((i & 15) == 0)
                        fprintf(cfile, "  \"");
                    fprintf(cfile, "\\x%02X", p[i] & 255);

                    if (i < r - 1 && (i & 15) == 15)
                        fprintf(cfile, "\"\n");

                }
                fprintf(cfile, "\"\n");
            }
            num++;
            if (verbose)
                printf("\n");
        }
        if (cfile)
            fprintf(cfile, "};\n");
        fclose(inf);
    }
    {
        WRBUF wrbuf = wrbuf_alloc();
        yaz_marc_write_trailer(mt, wrbuf);
        fputs(wrbuf_cstr(wrbuf), stdout);
        wrbuf_destroy(wrbuf);
    }
    if (cd)
        yaz_iconv_close(cd);
    yaz_marc_destroy(mt);
}

int main (int argc, char **argv)
{
    int r;
    int print_offset = 0;
    char *arg;
    int verbose = 0;
    int no = 0;
    int output_format = YAZ_MARC_LINE;
    FILE *cfile = 0;
    char *from = 0, *to = 0;
    int input_format = YAZ_MARC_ISO2709;
    int split_chunk = 1;
    const char *split_fname = 0;
    const char *leader_spec = 0;
    int write_using_libxml2 = 0;

#if HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif
#if HAVE_LANGINFO_H
#ifdef CODESET
    to = nl_langinfo(CODESET);
#endif
#endif

    prog = *argv;
    while ((r = options("i:o:C:npc:xOeXIf:t:s:l:Vv", argv, argc, &arg)) != -2)
    {
        no++;
        switch (r)
        {
        case 'i':
            input_format = yaz_marc_decode_formatstr(arg);
            if (input_format == -1)
            {
                fprintf(stderr, "%s: bad input format: %s\n", prog, arg);
                exit(1);
            }
#if YAZ_HAVE_XML2
#else
            if (input_format == YAZ_MARC_MARCXML
                || input_format == YAZ_MARC_XCHANGE)
            {
                fprintf(stderr, "%s: Libxml2 support not enabled\n", prog);
                exit(3);
            }
#endif
            break;
        case 'o':
            /* dirty hack so we can make Libxml2 do the writing ..
               rather than WRBUF */
            if (strlen(arg) > 4 && strncmp(arg, "xml,", 4) == 0)
            {
                /* Only supported for Libxml2 2.6.0 or later */
#if LIBXML_VERSION >= 20600
                arg = arg + 4;
                write_using_libxml2 = 1;
#else
                fprintf(stderr, "%s: output using Libxml2 unsupported\n", prog);
                exit(4);
#endif
            }
            output_format = yaz_marc_decode_formatstr(arg);
            if (output_format == -1)
            {
                fprintf(stderr, "%s: bad output format: %s\n", prog, arg);
                exit(1);
            }
            break;
        case 'l':
            leader_spec = arg;
            break;
        case 'f':
            from = arg;
            break;
        case 't':
            to = arg;
            break;
        case 'c':
            if (cfile)
                fclose(cfile);
            cfile = fopen(arg, "w");
            break;
        case 'x':
            fprintf(stderr, "%s: -x no longer supported. "
                    "Use -i marcxml instead\n", prog);
            exit(1);
            break;
        case 'O':
            fprintf(stderr, "%s: OAI MARC no longer supported."
                    " Use MARCXML instead.\n", prog);
            exit(1);
            break;
        case 'e':
            fprintf(stderr, "%s: -e no longer supported. "
                    "Use -o marcxchange instead\n", prog);
            exit(1);
            break;
        case 'X':
            fprintf(stderr, "%s: -X no longer supported. "
                    "Use -o marcxml instead\n", prog);
            exit(1);
            break;
        case 'I':
            fprintf(stderr, "%s: -I no longer supported. "
                    "Use -o marc instead\n", prog);
            exit(1);
            break;
        case 'n':
            output_format = YAZ_MARC_CHECK;
            break;
        case 'p':
            print_offset = 1;
            break;
        case 's':
            split_fname = arg;
            break;
        case 'C':
            split_chunk = atoi(arg);
            break;
        case 0:
            dump(arg, from, to, input_format, output_format,
                 write_using_libxml2,
                 print_offset, split_fname, split_chunk,
                 verbose, cfile, leader_spec);
            break;
        case 'v':
            verbose++;
            break;
        case 'V':
            show_version();
            break;
        default:
            usage(prog);
            exit(1);
        }
    }
    if (cfile)
        fclose(cfile);
    if (!no)
    {
        usage(prog);
        exit(1);
    }
    if (no_errors)
        exit(5);
    exit(0);
}
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

