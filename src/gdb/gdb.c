#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <netinet/tcp.h>

#include "gdb.h"
#include "breakpoints.h"
#ifdef __TARGET32
#include "target32.h"
#endif
#ifdef __TARGET64
#include "target64.h"
#endif

static int isServerStarted = 0;
static int fd_dbg = -1;

static void gdb_startServer(int portNb)
{
    int fd, ret, len;
    struct sockaddr_in sockaddr;
    int optval = 1;

    //create socket
    fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

    //bind it to port
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(portNb);
    sockaddr.sin_addr.s_addr = 0;
    ret = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    assert(ret == 0);

    //listen on socket
    ret = listen(fd, 0);
    assert(ret == 0);

    //wait for connection
    while(1) {
        len = sizeof(sockaddr);
        fd_dbg = accept(fd, (struct sockaddr *)&sockaddr, (socklen_t *) &len);
        if (fd_dbg < 0 && fd_dbg != -EINTR) {
            assert(0);
        } else if (fd_dbg >= 0) {
            break;
        }
    }

    fprintf(stderr, "Connect !!!!\n");
    fcntl(fd_dbg, F_SETFL, O_NONBLOCK);
    isServerStarted = 1;
}

static int gdb_calculate_checksum(char *buf, int len)
{
    int i;
    int res = 0;

    for(i = 0; i < len; i++) {
        res += buf[i];
    }

    return res;
}

static void gdb_send_packet(char *packet, int len)
{
    int ret;

    while (len > 0) {
        ret = send(fd_dbg, packet, len, 0);
        if (ret < 0) {
            if (ret != -EINTR && ret != -EAGAIN)
                assert(0);
        } else {
            packet += ret;
            len -= ret;
        }
    }
}

static void gdb_send_reply(char *reply)
{
    char buf[1024+5];
    int len = strlen(reply);
    int checksum = gdb_calculate_checksum(reply, len);

    buf[0] = '$';
    memcpy(&buf[1], reply, len);
    buf[len + 1] = '#';
    buf[len + 2] = gdb_tohex((checksum >> 4) & 0xf);
    buf[len + 3] = gdb_tohex((checksum) & 0xf);
    buf[len + 4] = '\0';
    //fprintf(stderr, "reply : %s\n", buf);
    gdb_send_packet(buf, len + 4);
}

static void gdb_read_memory(char *buf, unsigned long long int addr, unsigned long long int len)
{
    int i;

    if (addr >= 0 && addr < 4096) {
        sprintf(buf, "E14");
    } else {
        for(i = 0; i < len; i++) {
            unsigned char value = *((unsigned char *) g_2_h((guest_ptr) (addr + i)));
            unsigned int hnibble = (value >> 4);
            unsigned int lnibble = (value & 0xf);

            buf[1] = gdb_tohex(lnibble);
            buf[0] = gdb_tohex(hnibble);

            buf += 2;
        }
        *buf = '\0';
    }
}

static void gdb_new_command(struct gdb *gdb)
{
    char *cmd = gdb->command;
    char buf[1024];

    if (*cmd != 'm')
        ;//fprintf(stderr, "command : %s\n", gdb->command);

    buf[0] = '\0';
    switch(*cmd) {
        case '?':
            sprintf(buf, "S05");
            gdb_send_reply(buf);
            break;
        case 'q':
            if (strcmp(&cmd[1],"C") == 0) {
                sprintf(buf, "QC1");
            } else {
                buf[0] = '\0';
            }
            gdb_send_reply(buf);
            break;
        case 'g':
            gdb->read_registers(gdb, buf);
            gdb_send_reply(buf);
            break;
        case 'm':
            {
                char *next;
                unsigned long long int addr = strtoull(&cmd[1], &next, 16);
                unsigned long long int len;

                next++;
                len = strtoull(next, NULL, 16);
                gdb_read_memory(buf, addr, len);
            }
            gdb_send_reply(buf);
            break;
        case 's':
            {
                gdb->isSingleStepping = 1;
                gdb->isContinue = 1;
            }
            break;
        case 'c':
            {
                gdb->isSingleStepping = 0;
                gdb->isContinue = 1;
            }
            break;
        case 'Z':
        case 'z':
            {
                unsigned long long int type, addr, len;
                char *next;

                type = strtoul(&cmd[1], &next, 16);
                next++;
                addr = strtoull(next, &next, 16);
                next++;
                len = strtoull(next, NULL, 16);
                if (*cmd == 'Z')
                    gdb_insert_breakpoint(addr, len, type);
                else
                    gdb_remove_breakpoint(addr, len, type);
                sprintf(buf, "OK");
            }
            gdb_send_reply(buf);
            break;
        /*case 'v':
            if (strncmp(&cmd[1], "Cont", 4) == 0) {
                if (cmd[5] == '?') {
                    sprintf(buf, "vCont;c;C;s;S");
                    gdb_send_reply(tracee, buf);
                } else if (cmd[6] == 'c') {
                    tracee->gdb.isSingleStepping = false;
                    tracee->gdb.isContinue = true;
                } else if (cmd[6] == 's') {
                    tracee->gdb.isSingleStepping = true;
                    tracee->gdb.isContinue = true;
                }
            } else {
                buf[0] = '\0';
                gdb_send_reply(tracee, buf);
            }
            break;*/
        case 'k':
            fprintf(stderr, "\nUMEQ: Terminated via GDBstub\n");
            exit(0);
            break;
        default:
            buf[0] = '\0';
            gdb_send_reply(buf);
    }
}

static void gdb_new_data(struct gdb *gdb, char *buf, int nbBytes)
{
    int i;
    char c;

    for(i = 0; i < nbBytes; i++) {
        char ch = buf[i];
        switch(gdb->state) {
            case GDB_STATE_SYNCHRO:
                if (ch == '$') {
                    gdb->state = GDB_STATE_DATA;
                    gdb->commandPos = 0;
                }
                break;
            case GDB_STATE_DATA:
                if (ch == '#')
                    gdb->state = GDB_STATE_CHECKSUM_1;
                else
                    gdb->command[gdb->commandPos++] = ch;
                break;
            case GDB_STATE_CHECKSUM_1:
                gdb->command[gdb->commandPos] = '\0';
                gdb->state = GDB_STATE_CHECKSUM_2;
                break;
            case GDB_STATE_CHECKSUM_2:
                gdb->state = GDB_STATE_SYNCHRO;
                c = '+';
                gdb_send_packet(&c, 1);
                gdb_new_command(gdb);
                break;
            default:
                assert(0);
        }
    }
}

/* api */
int gdb_tohex(int v)
{
    if (v < 10)
        return v + '0';
    else
        return v - 10 + 'a';
}

int gdb_handle_breakpoint(struct gdb *gdb)
{
    char buf[16];

    //fprintf(stderr, "enter gdb_handle_breakpoint\n");
    if (!isServerStarted)
        gdb_startServer(1234);
    sprintf(buf, "S05");
    gdb_send_reply(buf);
    while(!gdb->isContinue) {
        int n;
        char buf[256];

        n = read(fd_dbg, buf, 256);
        if (n > 0)
            gdb_new_data(gdb, buf, n);
    }
    gdb->isContinue = 0;

    //fprintf(stderr, "exit gdb_handle_breakpoint\n");

    return 0;
}
