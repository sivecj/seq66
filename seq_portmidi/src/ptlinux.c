/*
 *  This file is part of seq66, adapted from the PortMIDI project.
 *
 *  seq66 is free software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  seq66 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with seq66; if not, write to the Free Software Foundation, Inc., 59 Temple
 *  Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file        ptlinux.c
 *
 *      Portable timer implementation for Linux.
 *
 * \library     seq66 application
 * \author      PortMIDI team; modifications by Chris Ahlstrom
 * \date        2017-08-21
 * \updates     2020-07-23
 * \license     GNU GPLv2 or above
 *
 * Implementation Notes (by Mark Nelson):
 *
 *      Unlike Windows, Linux has no system call to request a periodic
 *      callback, so if Pt_Start() receives a callback parameter, it must
 *      create a thread that wakes up periodically and calls the provided
 *      callback function.  If running as superuser, use setpriority() to
 *      renice thread to -20.  One could also set the timer thread to a
 *      real-time priority (SCHED_FIFO and SCHED_RR), but this is dangerous
 *      for This is necessary because if the callback hangs it'll never
 *      return. A more serious reason is that the current scheduler
 *      implementation busy-waits instead of sleeping when realtime threads
 *      request a sleep of <=2ms (as a way to get around the 10ms
 *      granularity), which means the thread would never let anyone else on
 *      the CPU.
 *
 * Change Log:
 *
 * 18-Jul-03 Roger Dannenberg -- Simplified code to set priority of timer
 *          thread. Simplified implementation notes.
 *
 * stdlib, stdio, unistd, and sys/types were added because they appeared
 * in a Gentoo patch, but I'm not sure why they are needed. -RBD
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/timeb.h>
#include <pthread.h>

#include "porttime.h"

/*
 * REDUNDANT
 */

#define TRUE        1
#define FALSE       0

static int time_started_flag = FALSE;
static struct timeb time_offset = { 0, 0, 0, 0 };
static pthread_t pt_thread_pid;
static int pt_thread_created = FALSE;

/*
 * note that this is static data -- we only need one copy
 */

typedef struct
{
    int id;
    int resolution;
    PtCallback * callback;
    void * userData;

} pt_callback_parameters;

/**
 *  Holds an ID to a callback function.
 */

static int pt_callback_proc_id = 0;

/**
 *  The ftime() function, which returns he current tim in seconds and
 *  milliseconds since the Epoch, is deprecated in favor or clock_gettime(2).
 *
 *      struct timeb time_offset =
 *      {
 *          0, // time
 *          0, // millitm
 *          0, // timezone
 *          0  // dstflag
 *      };
 */

void
Pt_ftime (struct timeb * tp)
{
    struct timespec temptime;
    int rc = clock_gettime(CLOCK_REALTIME_COARSE, &temptime);
    if (rc == 0)
    {
        tp->time = temptime.tv_sec;
        tp->millitm = temptime.tv_nsec / 1000000;
    }
    else
    {
        tp->time = 0;
        tp->millitm = 0;
    }
    tp->timezone = 0;
    tp->dstflag = 0;
}

/**
 *  To kill a process, just increment the pt_callback_proc_id.
 *
 * printf("pt_callback_proc_id %d, id %d\n", pt_callback_proc_id, parameters->id);
 */

static void *
Pt_CallbackProc (void * p)
{
    pt_callback_parameters * parameters = (pt_callback_parameters *) p;
    int mytime = 1;
    if (geteuid() == 0)
        setpriority(PRIO_PROCESS, 0, -20);

    while (pt_callback_proc_id == parameters->id)
    {
        /* wait for a multiple of resolution ms */

        struct timeval timeout;
        int delay = mytime++ * parameters->resolution - Pt_Time();
        if (delay < 0)
            delay = 0;

        timeout.tv_sec = 0;
        timeout.tv_usec = delay * 1000;
        select(0, NULL, NULL, NULL, &timeout);
        (*(parameters->callback))(Pt_Time(), parameters->userData);
    }
    return NULL;
}

/**
 *
 */

PtError
Pt_Start (int resolution, PtCallback * callback, void * userData)
{
    if (time_started_flag)
        return ptNoError;

    /*
     * Need this set before process runs.
     */

    Pt_ftime(&time_offset);             // ftime(&time_offset);
    if (callback)
    {
        int res;
        pt_callback_parameters * lparms = (pt_callback_parameters *)
            malloc(sizeof(pt_callback_parameters));

        if (! lparms)
            return ptInsufficientMemory;

        lparms->id = pt_callback_proc_id;
        lparms->resolution = resolution;
        lparms->callback = callback;
        lparms->userData = userData;
        res = pthread_create(&pt_thread_pid, NULL, Pt_CallbackProc, lparms);
        if (res != 0)
            return ptHostError;

        pt_thread_created = TRUE;
    }
    time_started_flag = TRUE;
    return ptNoError;
}

/**
 *
 */

PtError
Pt_Stop (void)
{
    pt_callback_proc_id++;
    if (pt_thread_created)
    {
        pthread_join(pt_thread_pid, NULL);
        pt_thread_created = FALSE;
    }
    time_started_flag = FALSE;
    return ptNoError;
}

/**
 *
 */

int
Pt_Started ()
{
    return time_started_flag;
}

/**
 *
 */

PtTimestamp
Pt_Time ()
{
    long seconds, milliseconds;
    struct timeb now;
    Pt_ftime(&now);                                     // ftime(&now);
    seconds = now.time - time_offset.time;
    milliseconds = now.millitm - time_offset.millitm;
    return seconds * 1000 + milliseconds;
}

/**
 *
 */

void
Pt_Sleep (int32_t duration)
{
    usleep(duration * 1000);
}

/*
 * ptlinux.c
 *
 * vim: sw=4 ts=4 wm=4 et ft=c
 */

