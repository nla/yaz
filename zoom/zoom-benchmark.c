/*
 * $Id: zoom-benchmark.c,v 1.1 2005-09-07 13:45:15 marc Exp $
 *
 * Asynchronous multi-target client doing search and piggyback retrieval
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include <yaz/xmalloc.h>
#include <yaz/zoom.h>




int main(int argc, char **argv)
{

    struct timeval now;
    struct timeval then;
    long sec=0, usec=0;

    int i;
    int same_target = 0;
    int no = argc-2;
    ZOOM_connection z[1024]; /* allow at most 1024 connections */
    ZOOM_resultset r[1024];  /* and result sets .. */
    ZOOM_options o = ZOOM_options_create ();

    if (argc < 3)
    {
        fprintf (stderr, "usage:\n%s target1 target2 ... targetN query\n",
                 *argv);
        exit (1);
    }
    if (argc == 4 && isdigit(argv[1][0]) && !strchr(argv[1],'.'))
    {
        no = atoi(argv[1]);
        same_target = 1;
    }

    if (no > 500)
        no = 500;

    /* naming events */
    
    //typedef enum {
    //    ZOOM_EVENT_NONE = 0,
    //    ZOOM_EVENT_CONNECT = 1,
    //    ZOOM_EVENT_SEND_DATA  = 2,
    //    ZOOM_EVENT_RECV_DATA = 3,
    //    ZOOM_EVENT_TIMEOUT = 4,
    //    ZOOM_EVENT_UNKNOWN = 5,
    //    ZOOM_EVENT_SEND_APDU = 6,
    //    ZOOM_EVENT_RECV_APDU = 7,
    //    ZOOM_EVENT_RECV_RECORD = 8,
    //    ZOOM_EVENT_RECV_SEARCH = 9
    //} ZOOM_events;

    char* zoom_events[10];
    zoom_events[ZOOM_EVENT_NONE] = "ZOOM_EVENT_NONE";
    zoom_events[ZOOM_EVENT_CONNECT] = "ZOOM_EVENT_CONNECT";
    zoom_events[ZOOM_EVENT_SEND_DATA] = "ZOOM_EVENT_SEND_DATA";
    zoom_events[ZOOM_EVENT_RECV_DATA] = "ZOOM_EVENT_RECV_DATA";
    zoom_events[ZOOM_EVENT_TIMEOUT] = "ZOOM_EVENT_TIMEOUT";
    zoom_events[ZOOM_EVENT_UNKNOWN] = "ZOOM_EVENT_UNKNOWN";
    zoom_events[ZOOM_EVENT_SEND_APDU] = "ZOOM_EVENT_SEND_APDU";
    zoom_events[ZOOM_EVENT_RECV_APDU] = "ZOOM_EVENT_RECV_APDU";
    zoom_events[ZOOM_EVENT_RECV_RECORD] = "ZOOM_EVENT_RECV_RECORD";
    zoom_events[ZOOM_EVENT_RECV_SEARCH] = "ZOOM_EVENT_RECV_SEARCH";

    int zoom_progress[10];
    zoom_progress[ZOOM_EVENT_NONE] = 0;
    zoom_progress[ZOOM_EVENT_CONNECT] = 1;
    zoom_progress[ZOOM_EVENT_SEND_DATA] = 3;
    zoom_progress[ZOOM_EVENT_RECV_DATA] = 4;
    zoom_progress[ZOOM_EVENT_TIMEOUT] = 8;
    zoom_progress[ZOOM_EVENT_UNKNOWN] = 9;
    zoom_progress[ZOOM_EVENT_SEND_APDU] = 2;
    zoom_progress[ZOOM_EVENT_RECV_APDU] = 5;
    zoom_progress[ZOOM_EVENT_RECV_RECORD] = 7;
    zoom_progress[ZOOM_EVENT_RECV_SEARCH] = 6;
   

    /* async mode */
    ZOOM_options_set (o, "async", "1");

    /* get first 10 records of result set (using piggyback) */
    ZOOM_options_set (o, "count", "1");

    /* preferred record syntax */
    //ZOOM_options_set (o, "preferredRecordSyntax", "usmarc");
    //ZOOM_options_set (o, "elementSetName", "F");

    /* connect to all */
    for (i = 0; i<no; i++)
    {
        /* create connection - pass options (they are the same for all) */
        z[i] = ZOOM_connection_create (o);

        /* connect and init */
        if (same_target)
            ZOOM_connection_connect (z[i], argv[2], 0);
        else
            ZOOM_connection_connect (z[i], argv[1+i], 0);
    }
    /* search all */
    for (i = 0; i<no; i++)
        r[i] = ZOOM_connection_search_pqf (z[i], argv[argc-1]);

    printf ("second.usec target progress event eventname error errorname\n");


    gettimeofday(&then, 0);
    /* network I/O. pass number of connections and array of connections */
    while ((i = ZOOM_event (no, z)))
    { 

        int event = ZOOM_connection_last_event(z[i-1]);
        const char *errmsg;
        const char *addinfo;
        int error = 0;
        int progress = zoom_progress[event];

        gettimeofday(&now, 0);
        //stamp(&now, &then);
        sec = now.tv_sec - then.tv_sec;
        usec = now.tv_usec - then.tv_usec;
        if (usec < 0){
            sec--;
            usec += 1000000;
        }
 
        error = ZOOM_connection_error(z[i-1] , &errmsg, &addinfo);
        if (error)
            progress = -progress;
        
        printf ("%i.%06i %d %d %d %s %d '%s' \n", sec, usec, 
                i-1, progress,
                event, zoom_events[event], 
                error, errmsg);

    }
    
    /* no more to be done. Inspect results */
    for (i = 0; i<no && 0; i++)
    {
        int error;
        const char *errmsg, *addinfo;
        const char *tname = (same_target ? argv[2] : argv[1+i]);
        /* display errors if any */
        if ((error = ZOOM_connection_error(z[i], &errmsg, &addinfo)))
            fprintf (stderr, "%s error: %s (%d) %s\n", tname, errmsg,
                     error, addinfo);
        else
        {
            /* OK, no major errors. Look at the result count */
            int pos;
            printf ("%s: %d hits\n", tname, ZOOM_resultset_size(r[i]));
            /* go through all records at target */
            for (pos = 0; pos < 10; pos++)
            {
                int len; /* length of buffer rec */
                const char *rec =
                    ZOOM_record_get (
                        ZOOM_resultset_record (r[i], pos), "render", &len);
                /* if rec is non-null, we got a record for display */
                if (rec)
                {
                    printf ("%d\n", pos+1);
                    if (rec)
                        fwrite (rec, 1, len, stdout);
                    printf ("\n");
                }
            }
        }
    }

    /* destroy and exit */
    for (i = 0; i<no; i++)
    {
        ZOOM_resultset_destroy (r[i]);
        ZOOM_connection_destroy (z[i]);
    }
    ZOOM_options_destroy(o);
    exit (0);
}
/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
