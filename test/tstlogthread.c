/*
 * Copyright (c) 1998-2005, Index Data.
 * See the file LICENSE for details.
 * 
 * $Id: tstlogthread.c,v 1.2 2005-09-08 13:51:27 adam Exp $
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <yaz/log.h>
#include <yaz/options.h>

#if YAZ_POSIX_THREADS
#include <pthread.h>


static void *t_loop2(void *vp)
{
    int i, sz = 10;

    for (i = 0; i<sz; i++)
    {
#if 0
        fprintf(stderr, "pr %d\n", i);
#else
        yaz_log(YLOG_LOG, "pr %d", i);
#endif
    }
    return 0;
}

static void t_test()
{
    pthread_t tids[4];
    
    pthread_create(tids+0, 0, t_loop2, 0);
    pthread_create(tids+1, 0, t_loop2, 0);
    pthread_create(tids+2, 0, t_loop2, 0);
    pthread_create(tids+3, 0, t_loop2, 0);
    
    pthread_join(tids[0], 0);
    pthread_join(tids[1], 0);
    pthread_join(tids[2], 0);
    pthread_join(tids[3], 0);
    exit(0);
}
#else
static void t_test()
{
}

#endif

int main(int argc, char **argv)
{
    char *arg;
    int ret;

    while ((ret = options("v:l:", argv, argc, &arg)) != -2)
    {
        switch (ret)
        {
        case 'v':
            yaz_log_init_level (yaz_log_mask_str(arg));
            break;
        case 'l':
            yaz_log_init_file(arg);
            break;
        default:
            return 1;
        }
    }
    t_test();
    return 0;
}
/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
