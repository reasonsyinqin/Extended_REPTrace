#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#define PATHNAME "."
#define PROJ_ID 0x6666
#define BUFFLEN 2
#define SERVER_PORT 3212
#define BACKLOG 1000
#define PIDNUMB 100
// define register type
#define REG_QUERY 1
#define REG_REGISTER 2
#define REG_DEREGISTER 3

#define REGISTER_LENTH 4096
#define REGISTER_END -1224
#define REGISTER_INIT_FLAG -7871
// in register: flag 0 unused 1 used
// in communication: flag=register type (1,2,3)
typedef struct
{
    int flag;
    char ip[16];
    int port;
} peer;
peer *regaddr = NULL;
void showpeerstruct(peer *p)
{
    // printf("line=%d,func=%s\n", __LINE__, __func__);
    printf("flag=%d, ip=%s, port=%d\n", p->flag, p->ip, p->port);
}

peer peer_list[REGISTER_LENTH];
static int commShm(int size, int flags)
{
    key_t key = ftok(PATHNAME, PROJ_ID);
    if (key < 0)
    {
        perror("ftok");
        return -1;
    }
    int shmid = shmget(key, size, flags);
    if (shmid < 0)
    {
        perror("shmget");
        return -2;
    }
    return shmid;
}
int createShm(int size)
{
    return commShm(size, IPC_CREAT | IPC_EXCL);
}
int getShm(int size)
{
    return commShm(size, IPC_CREAT);
}

int initCentralRegister()
{
    int shmid = getShm(sizeof(peer) * REGISTER_LENTH);
    if (shmid < 0)
    {
        printf("errno=%d,strerror=%s\n", errno, strerror(errno));
    }
    printf("shmid=%d\n", shmid);
    regaddr = (peer *)shmat(shmid, NULL, 0);
    // printf("regaddr[REGISTER_LENTH - 1]=%d", regaddr[REGISTER_LENTH - 1]);
    if (regaddr[REGISTER_LENTH - 1].flag != REGISTER_INIT_FLAG)
    {

        int i;
        for (i = 0; i < REGISTER_LENTH; i++)
        {
            regaddr[i].flag = 0;
            strcpy(regaddr[i].ip, "");
            regaddr[i].port = 0;
        }

        regaddr[REGISTER_LENTH - 1].flag = REGISTER_INIT_FLAG;

        regaddr[1].flag = REGISTER_END;

        showpeerstruct(&regaddr[REGISTER_LENTH - 1]);
    }
}

int query(peer *p)
{
    int i;
    char ip[16];
    strcpy(ip, p->ip);
    int port = p->port;
    for (i = 0; regaddr[i].flag != REGISTER_END; i++)
    {
        // printf("%d ", regaddr[i]);
        if (regaddr[i].flag != 0 && regaddr[i].port == port && (strcmp(ip, regaddr[i].ip) == 0))
        {
            // printf("\nfind port=%d\n", port);
            return i;
        }
    }
    return -1;
}

int register_(peer *p)
{
    int i;
    int index = query(p);
    char ip[16];
    strcpy(ip, p->ip);
    int port = p->port;
    //not exist
    if (index == -1)
    {
        for (i = 0; regaddr[i].flag != REGISTER_END; i++)
        {
            if (regaddr[i].flag == 0)
            {
                strcpy(regaddr[i].ip, ip);
                regaddr[i].port = port;
                regaddr[i].flag = 1;
                if ((regaddr[i + 1].flag == REGISTER_END) && (regaddr[i + 2].flag == 0) && (regaddr[i + 3].flag == 0) && (regaddr[i + 4].flag == 0) && (regaddr[i + 5].flag == 0))
                { // register last
                    regaddr[i + 1].flag = 0;
                    regaddr[i + 2].flag = REGISTER_END;
                }
                return i;
            }
        }
    }
    else
    {
        return index;
    }

    return -1;
}

int deregister(peer *p)
{
    int i;
    int index = query(p);
    if (index == -1)
    { // not found
        return -1;
    }
    else
    {
        // printf("\nderegister port=%d\n", regaddr[index]);
        regaddr[index].flag = 0;
        memset(regaddr[index].ip, 0x00, sizeof(regaddr[index].ip));
        regaddr[index].port = 0;
        if ((regaddr[index + 1].flag == 0) && (regaddr[index + 2].flag == REGISTER_END) && (regaddr[index + 3].flag == 0) && (regaddr[index + 4].flag == 0) && (regaddr[index + 5].flag == 0))
        { // deregister last
            regaddr[index + 1].flag = REGISTER_END;
            regaddr[index + 2].flag = 0;
        }
        return index;
    }

    return -1;
}

void showreg()
{
    // printf("line=%d,func=%s\n", __LINE__, __func__);
    int i;
    for (i = 0; regaddr[i].flag != REGISTER_END; i++)
    {
        printf("regaddr[%d]: ", i);
        showpeerstruct(&regaddr[i]);
    }
    printf("regaddr[%d]: ", i);
    showpeerstruct(&regaddr[i]);
    printf("\n");
}

int reg(peer *p)
{
    int register_type = p->flag;
    if (register_type == REG_QUERY)
    {
        return query(p);
    }
    else if (register_type == REG_REGISTER)
    {
        // showreg();
        return register_(p);
    }
    else if (register_type == REG_DEREGISTER)
    {
        // showreg();
        return deregister(p);
    }
    return -1;
}

static void handle_connect(int s_s)
{
    int s_c;
    struct sockaddr_in from;
    socklen_t len = sizeof(from);

    while (1)
    {
        printf("========================================\n");
        s_c = accept(s_s, (struct sockaddr *)&from, &len);
        printf("accept successfully\n");
        time_t now;
        peer *mypeer = (peer *)malloc(sizeof(peer));

        int needrecv = sizeof(peer);
        char *buff = (char *)malloc(needrecv);
        int pos = 0;
        int n;
        int res = -1;
        memset(buff, 0, sizeof(buff));
        while (pos < needrecv)
        {
            n = recv(s_c, buff + pos, needrecv, 0);
            if (n <= 0)
            {
                printf("Server Recieve Data Failed!\n");
                break;
            }

            pos += n;
            printf("pos=%d,needrecv=%d\n", pos, needrecv);
        }
        printf("needrecv=%d,buff=%s\n", needrecv, buff);
        memcpy(mypeer, buff, needrecv);
        printf("flag=%d, ip=%s, port=%d\n", mypeer->flag, mypeer->ip, mypeer->port);
        showpeerstruct(mypeer);
        showreg();
        if (n > 0)
        {
            res = reg(mypeer);
            printf("reg()res=%d\n", res);
        }
        showreg();
        send(s_c, (char *)&res, sizeof(res), 0);
        printf("send res=%d\n", res);
        close(s_c);
        free(mypeer);
        free(buff);
        // showreg();
    }
}

void sig_int(int num)
{
    exit(1);
}
int main(int argc, char *argv[])
{
    int s_s;
    struct sockaddr_in local;
    signal(SIGINT, sig_int);
    initCentralRegister();
    s_s = socket(AF_INET, SOCK_STREAM, 0);

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    // local.sin_addr.s_addr = htonl(inet_addr("127.0.0.1"));
    local.sin_port = htons(SERVER_PORT);

    bind(s_s, (struct sockaddr *)&local, sizeof(local));
    listen(s_s, BACKLOG);
    printf("I am listening %d now\n",SERVER_PORT);
    pid_t pid[PIDNUMB];
    int i = 0;
    for (i = 0; i < PIDNUMB; i++)
    {
        pid[i] = fork();
        if (pid[i] == 0)
        {
            handle_connect(s_s);
        }
    }
    while (1)
        ;

    close(s_s);

    return 0;
}
