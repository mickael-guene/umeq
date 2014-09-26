#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <signal.h>
#include <time.h>
#include <assert.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

/* WARNING : timer_t is decalred as  (void *). So it's 8 bytes on 64 bits and 4 bytes on 32 bits.
             But it seems kernel just write low 4 bytes with a counter value. So we can directly
             map 32 bits arch timer_t onto 64 bits arch timer_t. BUT we are kernel implementation
             dependent .....
 */

int timer_create_s3264(uint32_t clockid_p, uint32_t sevp_p, uint32_t timerid_p)
{
	int res;
	clockid_t clockid = (clockid_t) clockid_p;
	struct sigevent_32 *sevp_guest = (struct sigevent_32 *) g_2_h(sevp_p);
	timer_t *timerid = (timer_t *) g_2_h(timerid_p);
	struct sigevent evp;

    switch(sevp_guest->sigev_notify) {
        case SIGEV_NONE:
            evp.sigev_notify = SIGEV_NONE;
            break;
        case SIGEV_SIGNAL:
            evp.sigev_notify = SIGEV_SIGNAL;
            evp.sigev_signo = sevp_guest->sigev_signo;
            evp.sigev_value.sival_ptr = (void *) g_2_h(sevp_guest->sigev_value.sival_ptr);
            break;
        default:
            /* FIXME : add SIGEV_THREAD + SIGEV_THREAD_ID support */
            assert(0);
    }

    res = syscall(SYS_timer_create, clockid, &evp, timerid);

    return res;
}

int timer_settime_s3264(uint32_t timerid_p, uint32_t flags_p, uint32_t new_value_p, uint32_t old_value_p)
{
	int res;
	timer_t timerid = (timer_t)(long) timerid_p;
	int flags = (int) flags_p;
	struct itimerspec_32 *new_value_guest = (struct itimerspec_32 *) g_2_h(new_value_p);
	struct itimerspec_32 *old_value_guest = (struct itimerspec_32 *) g_2_h(old_value_p);
	struct itimerspec new_value;
    struct itimerspec old_value;

    new_value.it_interval.tv_sec = new_value_guest->it_interval.tv_sec;
    new_value.it_interval.tv_nsec = new_value_guest->it_interval.tv_nsec;
    new_value.it_value.tv_sec = new_value_guest->it_value.tv_sec;
    new_value.it_value.tv_nsec = new_value_guest->it_value.tv_nsec;

    res = syscall(SYS_timer_settime, timerid, flags, &new_value, old_value_p?&old_value:NULL);

    if (old_value_p) {
        old_value_guest->it_interval.tv_sec = old_value.it_interval.tv_sec;
        old_value_guest->it_interval.tv_nsec = old_value.it_interval.tv_nsec;
        old_value_guest->it_value.tv_sec = old_value.it_value.tv_sec;
        old_value_guest->it_value.tv_nsec = old_value.it_value.tv_nsec;
    }

    return res;
}
