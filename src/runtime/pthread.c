/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <linux/futex.h>

static void futex_wait(int *addr)
{
    syscall(SYS_futex, addr, FUTEX_WAIT, 2, NULL);
}

static void futex_wake(int *addr)
{
    syscall(SYS_futex, addr, FUTEX_WAKE, 1);
}

static void ll_lock(int *addr)
{
    /* try to take futex */
    if (__sync_val_compare_and_swap(addr, 0, 1)) {
        /*  futex is already taken */
        do {
            /* clain there is at least one waiter */
            int oldval = __sync_val_compare_and_swap(addr, 1 , 2);
            /* don't wait if lock is now free */
            if (oldval)
                futex_wait(addr);
            /* try to take again the lock */
        } while (!__sync_bool_compare_and_swap(addr, 0, 2));
    }
}

static void ll_unlock(int *addr)
{
    int oldval;

    /* atomically set futex to zero to free futex */
    do {
        oldval = *addr;
    } while (!__sync_bool_compare_and_swap(addr, oldval, 0));

    /* do we have to wake up someone ? */
    if (oldval > 1)
        futex_wake(addr);
}

/* lock api */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    ll_lock(&(mutex)->__data.__lock);

    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    ll_unlock(&(mutex)->__data.__lock);

    return 0;
}
