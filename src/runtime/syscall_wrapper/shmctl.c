#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    return syscall(SYS_shmctl, shmid, cmd, buf);
}
