/* This file is part of the YAZ toolkit.
 * Copyright (C) 1995-2013 Index Data
 * See the file LICENSE for details.
 */

/**
 * \file matchstr.c
 * \brief a couple of string utilities
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <yaz/yaz-iconv.h>
#include <yaz/matchstr.h>

int yaz_matchstr(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        unsigned char c1 = *s1;
        unsigned char c2 = *s2;

        if (c2 == '?')
            return 0;
        if (c1 == '-')
            c1 = *++s1;
        if (c2 == '-')
            c2 = *++s2;
        if (!c1 || !c2)
            break;
        if (c2 != '.')
        {
            if (yaz_isupper(c1))
                c1 = yaz_tolower(c1);
            if (yaz_isupper(c2))
                c2 = yaz_tolower(c2);
            if (c1 != c2)
                break;
        }
        s1++;
        s2++;
    }
    return *s1 || *s2;
}

int yaz_strcmp_del(const char *a, const char *b, const char *b_del)
{
    while (*a && *b)
    {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    if (b_del && strchr(b_del, *b))
        return *a;
    return *a - *b;
}

int yaz_memcmp(const void *a, const void *b, size_t len_a, size_t len_b)
{
    size_t m_len = len_a < len_b ? len_a : len_b;
    int r = memcmp(a, b, m_len);
    if (r)
        return r;
    return len_a - len_b;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

