#include "tracer.h"

////

// 0 controller off
// 1 controller on
const short with_uuid = 1;
const int log_error_info = FALSE;

const char *filePath = "/lib64/logs/tracelog.txt";
const char *debugFilePath = "/lib64/logs/tracelog2.txt";
const char *dataFilePath = "/lib64/logs/traceData.dat";
const char *errFilePath = "/lib64/logs/traceErr.txt";

int traced_ip_list_length = 1;
char *traced_ip_list[] = {"127.0.0.1"};
#define not_traced_port_list_length 1
const int not_traced_port_list[not_traced_port_list_length] = {22};

const int inited_traced_port_list_length = 4;
const int inited_traced_port_list[] = {5671, 5672, 15672, 25672};

const char *logRule = "logs";

const char *idenv = "UUID_PASS";
static pthread_key_t uuid_key = 12345;
extern char **environ;
char tmp[1024];
int shmid = -1;

int is_client_connect = FALSE;
int is_server_listen = TRUE;
int is_server_accept = TRUE;

int client_connect_sockfd = -1;
int server_listen_sockfd = -1;
int server_accept_sockfd = -1;
// local register
quadruple *regaddr = NULL;
// with previous implementation and current read (without push)

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{

    init_context("recv");
    static void *handle = NULL;
    static RECV old_recv = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_recv = (RECV)dlsym(handle, "recv");
    }

#ifdef DEBUG
    char log_text[LOG_LENGTH] = "init";
    sprintf(log_text, "%s\t", "in recv");
#endif
    // check socket type first
    sa_family_t socket_family = get_socket_family(sockfd);
    // currently only support for IPv4
    //

    if (socket_family == AF_INET)
    {
#ifdef DEBUG
        sprintf(log_text, "%s%s\t", log_text, "AF_INET socket");
#endif
        int tmp_errno = 0;
        struct sockaddr_in sin; // local socket info
        struct sockaddr_in son; // remote socket into
        socklen_t s_len = sizeof(sin);
        unsigned short int in_port = 0;
        unsigned short int on_port = 0;
        char in_ip[256] = "0.0.0.0";
        char on_ip[256] = "0.0.0.0";

        if (getsockname(sockfd, (struct sockaddr *)&sin, &s_len) != -1)
        {
            char *tmp_in_ip;
            in_port = ntohs(sin.sin_port);
            tmp_in_ip = inet_ntoa(sin.sin_addr);
            memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getsock_recv");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getsock_recv");
#endif
        }
        // no connection, actually for recv, no connection now already means error
        if (getpeername(sockfd, (struct sockaddr *)&son, &s_len) != -1)
        {
            char *tmp_on_ip;
            on_port = ntohs(son.sin_port);
            tmp_on_ip = inet_ntoa(son.sin_addr);
            memmove(on_ip, tmp_on_ip, strlen(tmp_on_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getpeer_recv");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getpeer_recv");
#endif
        }

#ifdef IP_PORT
        sprintf(log_text, "%srecv from %s:%d with %s:%d\t", log_text, on_ip, on_port, in_ip, in_port);
#endif

        // actually this information is currently unused
#ifdef DATA_INFO
        // get socket buff size
        int recv_size = 0; // socket接收缓冲区大小
        int send_size = 0;
        socklen_t optlen;
        optlen = sizeof(send_size);
        if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket send buff failed!");
        }
        optlen = sizeof(recv_size);
        if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket recv buff failed!");
        }
#endif
#ifdef DEBUG
        sprintf(log_text, "%srecv process:%u\t thread:%lu\t socketid:%d\trequired:%lu\t", log_text, getpid(), pthread_self(), sockfd, len);
#endif

        int local_should_be_traced = check_if_local_should_be_traced(in_ip, in_port);

        if (!local_should_be_traced)
        {

            return old_recv(sockfd, buf, len, flags);
        }

        char uuids[LOG_LENGTH] = "";
        ssize_t n = 0;
        ssize_t header_read_len = 0;
        size_t msg_orig_length = 0;
        ssize_t consumed_bytes = 0;

        // now this local process should be REPTraced, i.e. we should insert MSG_ID and MSG_CTX_ID.
        // if now handling message fragment, continue the handling

        int handle_fragment = check_if_handle_fragment(sockfd);

        if (handle_fragment > 0)
        {
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,handle_fragment=%d,len=%d", __LINE__, __func__, handle_fragment, len);
                log_important(tmp);
            }
            int left = handle_fragment;
            n = check_read_rest(buf, sockfd, left, len, flags);
            tmp_errno = errno;
            errno = 0; // not sure necessary or not
#ifdef DEBUG
            sprintf(log_text, "%sresult:%ld\n", log_text, n);
            log_event(log_text);
#endif
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_RECV, n, len, n, sockfd, RECV_LEFT, buf);
            errno = tmp_errno;
            return n;
        }
        if (handle_fragment < 0)
        {
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,handle_fragment=%d,len=%d", __LINE__, __func__, handle_fragment, len);
                log_important(tmp);
            }
            return old_recv(sockfd, buf, len, flags);
        }

        // now handle the entire message from beginning
        int remote_traced = query_reg_remote_peer_traced(in_ip, in_port, on_ip, on_port);
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,n=%d,remote_traced=%d,in_port=%d,on_port=%d,len=%d", __LINE__, __func__, n, remote_traced, in_port, on_port, len);
            log_important(tmp);
        }
        if (remote_traced)
        {
            // read header(38 bytes)
            if (len < ID_LENGTH)
            {
                header_read_len = old_recv(sockfd, uuids, len, flags);
            }
            else
            {
                header_read_len = old_recv(sockfd, uuids, ID_LENGTH, flags);
            }
            if (uuids[0] == '@')
            {
                if ((header_read_len != -1) && (header_read_len != ID_LENGTH) && (header_read_len != 0))
                { // header_read_len<ID_LENGTH
                    if (log_error_info)
                    {
                        sprintf(tmp, "Log,line=%d,func=%s,header_read_len=%d,tmp_errno=%d,len=%d", __LINE__, __func__, header_read_len, tmp_errno, len);
                        log_important(tmp);
                    }
                    ssize_t tmpValue = 0;
                    while ((header_read_len != ID_LENGTH) && ((tmp_errno == 0) || (tmp_errno == EAGAIN) || (tmp_errno == EWOULDBLOCK)))
                    {
                        tmpValue = old_recv(sockfd, &uuids[header_read_len], ID_LENGTH - header_read_len, flags);
                        tmp_errno = errno;

                        if (tmpValue > 0)
                        {
                            if (header_read_len > 0)
                            {
                                header_read_len += tmpValue;
                            }
                            else
                            {
                                header_read_len = tmpValue;
                            }
                        }
                        else if (tmpValue == 0)
                        {
                            log_important("Packet length less than 38 bytes!");
                            break;
                        }
                    }
                }
            }
            if (header_read_len < 0)
            {
                char f_type = RECV_HEADERR;
                tmp_errno = errno;
                errno = 0;
                push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_RECV, header_read_len, len, header_read_len, sockfd, f_type, NULL);
                errno = tmp_errno;
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,header_read_len=%d,tmp_errno=%d,len=%d", __LINE__, __func__, header_read_len, tmp_errno, len);
                    log_important(tmp);
                }
                return header_read_len;
            }
            else if (header_read_len < ID_LENGTH)
            { // not header, and len<ID_LENGTH
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,header_read_len=%d,tmp_errno=%d,len=%d", __LINE__, __func__, header_read_len, tmp_errno, len);
                    log_important(tmp);
                }
                memmove(buf, uuids, len);
                tmp_errno = errno;
                push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_RECV, ID_LENGTH, len, len, sockfd, RECV_FAIL, buf);
                errno = tmp_errno;
                return len;
            }
            else if (header_read_len == ID_LENGTH)
            {
                if ((uuids[0] == '@') && (uuids[ID_LENGTH - 2] == '@') && (uuids[ID_LENGTH - 1] == '\0'))
                { // find header

                    *(&msg_orig_length) = *((size_t *)&uuids[1]);
                    int i = 0;
                    for (; i < sizeof(msg_orig_length); i++)
                    {
                        uuids[i + 1] = '-'; // just for print
                    }
                    errno = 0;
                    size_t tmp_len = msg_orig_length;
                    // read rest

                    n = check_read_rest(buf, sockfd, msg_orig_length, len, flags);
                    tmp_errno = errno;

                    errno = 0;
                    char tmp_id[ID_LENGTH];
                    format_uuid(uuids, tmp_id);
                    push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), tmp_id, gettime(), F_RECV, ID_LENGTH + n, len, n, sockfd, RECV_ID, buf);
                    if (log_error_info)
                    {

                        sprintf(tmp, "Log,line=%d,func=%s,register_line=%d,errno=%d,on_ip=%s, on_port=%d,in_ip=%s,in_port=%d, n=%d,sockfd=%d, msg_orig_length=%d, flags=%d,header_read_len=%d ", __LINE__, __func__, remote_traced, errno, on_ip, on_port, in_ip, in_port, n, sockfd, msg_orig_length, flags, header_read_len);
                        log_important(tmp);
                    }
                    errno = tmp_errno;
                    return n;
                }
                else
                { // if it's not ID
                    msg_orig_length = 0;
                    if (header_read_len > len)
                    {
                        log_important("fatal_bigger_");
                        memmove(buf, uuids, len);
                        tmp_errno = errno;
                        push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_RECV, ID_LENGTH, len, len, sockfd, RECV_FAIL, buf);
                        errno = tmp_errno;
                        return len;
                    }
                    else
                    {
                        memmove(buf, uuids, header_read_len);
                        ssize_t second = old_recv(sockfd, &buf[header_read_len], len - header_read_len, flags);
                        if (second <= 0)
                        {
                            log_important("note_noid_error_recv");
                        }
                        else
                        {
                            n = header_read_len + second;
                        }
                        tmp_errno = errno;
                        push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_RECV, n, len, n, sockfd, RECV_BYD, buf);
                        errno = tmp_errno;
                        return n;
                    }
                    log_message(uuids, ID_LENGTH, "check1");
                }
            }
            tmp_errno = errno;

            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,header_read_len=%d,tmp_errno=%d,len=%d", __LINE__, __func__, header_read_len, tmp_errno, len);
                log_important(tmp);
            }
            // read 38 bytes
        }
        else
        {
            n = old_recv(sockfd, buf, len, flags);
            if (log_error_info)
            {
                sprintf(tmp, "errno=%d,Log,line=%d,func=%s,socket_family=%d,n=%d", errno, __LINE__, __func__, socket_family, n);
                log_important(tmp);
            }
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_RECV, n, len, n, sockfd, RECV_ID, buf);
            return n;
        }
    }
    return old_recv(sockfd, buf, len, flags);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    init_context("send");
    ;
    static void *handle = NULL;
    static SEND old_send = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_send = (SEND)dlsym(handle, "send");
    }
#ifdef DEBUG
    char log_text[LOG_LENGTH] = "init";
    sprintf(log_text, "%s\t", "in send");
#endif

    // check socket type first
    sa_family_t socket_family = get_socket_family(sockfd);
    if (socket_family == AF_INET)
    {
#ifdef DEBUG
        sprintf(log_text, "%s%s\t", log_text, "AF_INET socket");
#endif
        int tmp_erro = 0;
        struct sockaddr_in sin; // local socket info
        struct sockaddr_in son; // remote socket into
        socklen_t s_len = sizeof(sin);
        unsigned short int in_port = 0;
        unsigned short int on_port = 0;
        char in_ip[256] = "0.0.0.0";
        char on_ip[256] = "0.0.0.0";
        if (getsockname(sockfd, (struct sockaddr *)&sin, &s_len) != -1)
        {
            char *tmp_in_ip;
            in_port = ntohs(sin.sin_port);
            tmp_in_ip = inet_ntoa(sin.sin_addr);
            memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getsock_send");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getsock_send");
#endif
        }

        // no connection, actually for recv, no connection now already means error
        if (getpeername(sockfd, (struct sockaddr *)&son, &s_len) != -1)
        {
            char *tmp_on_ip;
            on_port = ntohs(son.sin_port);
            tmp_on_ip = inet_ntoa(son.sin_addr);
            memmove(on_ip, tmp_on_ip, strlen(tmp_on_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getpeer_send");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getpeer_send");
#endif
        }

#ifdef IP_PORT
        sprintf(log_text, "%ssend to %s:%d with %s:%d\t", log_text, on_ip, on_port, in_ip, in_port);
#endif

        // actually this information is currently unused
#ifdef DATA_INFO
        // get socket buff size
        int recv_size = 0; // socket接收缓冲区大小
        int send_size = 0;
        socklen_t optlen;
        optlen = sizeof(send_size);
        if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket send buff failed!");
        }
        optlen = sizeof(recv_size);
        if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket recv buff failed!");
        }
#endif

        int local_should_be_traced = check_if_local_should_be_traced(in_ip, in_port);
        if (!local_should_be_traced)
        {
            return old_send(sockfd, buf, len, flags);
        }

        ssize_t n = 0;

        // now handle the entire message from beginning
        int remote_traced = query_reg_remote_peer_traced(in_ip, in_port, on_ip, on_port);
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,n=%d,remote_traced=%d,in_port=%d,on_port=%d,len=%d", __LINE__, __func__, n, remote_traced, in_port, on_port, len);
            log_important(tmp);
        }

        if (remote_traced)
        {
            // need to prepare the REPTrace header, and then send out the header and the message
            size_t new_len = len + ID_LENGTH;
            char f_type = -1;
            char target[new_len];
            char id[ID_LENGTH];
            random_uuid(id, len);

            char tmp_id[ID_LENGTH];
            format_uuid(id, tmp_id);
            memmove(&target, id, ID_LENGTH);
            memmove(&target[ID_LENGTH], buf, len);
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,n=%d,remote_traced=%d,in_port=%d,on_port=%d,len=%d,target[1]=%d", __LINE__, __func__, n, remote_traced, in_port, on_port, len, target[1]);
                log_important(tmp);
            }
            ssize_t rvalue = 0;
            tmp_erro = errno;
            errno = 0;
            long long ttime;
            ttime = gettime();
            n = old_send(sockfd, target, new_len, flags);
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,remote_traced=%d,new_len=%d,n=%d,target=", __LINE__, __func__, remote_traced, new_len, n);
                log_important(tmp);
                log_important(target);
            }
            tmp_erro = errno;
            errno = 0;
            if (n == new_len)
            {
                rvalue = len;
                f_type = SEND_ID;
#ifdef DEBUG
                sprintf(log_text, "%suuid:%s\tnormal1\tresult:%ld\n", log_text, tmp_id, rvalue);
                log_event(log_text);
#endif
            }
            else if (n == 0)
            {
#ifdef DEBUG
                sprintf(log_text, "%suuid:%s\tdisconnect\tresult:%ld\n", log_text, id, rvalue);
                log_event(log_text);
#endif
                f_type = SEND_DISCONN;
                rvalue = 0;
            }
            else
            {
                // create fragments if one-time send() does not transmit out all data
                tmp_erro = errno;
                while ((n != new_len) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
                {
                    ssize_t tmpValue = 0;
                    if (n != -1)
                    {
                        tmpValue = old_send(sockfd, &target[n], new_len - n, flags);
                    }
                    else
                    {
                        tmpValue = old_send(sockfd, &target[0], new_len, flags);
                    }
                    tmp_erro = errno;
                    if (tmpValue >= 0)
                    {
                        n += tmpValue;
                    }
                }
                if (n == new_len)
                {
#ifdef DEBUG
                    sprintf(log_text, "%suuid:%s\tnormal2\tresult:%ld\n", log_text, tmp_id, len);
#endif
                    f_type = SEND_ID;
                    rvalue = len;
                }
                else if (n == -1)
                {
#ifdef DEBUG
                    sprintf(log_text, "%sfinal err:%d\tsend short error\tresult:%ld\n", log_text, tmp_erro, -1L);
#endif
                    f_type = SEND_ERROR;
                    errno = tmp_erro;
                    rvalue = -1;
                }
                else if ((n <= new_len) && (n >= ID_LENGTH))
                {
#ifdef DEBUG
                    sprintf(log_text, "%sfinal err:%d\tsend short1 error\tresult:%ld\n", log_text, tmp_erro, n - ID_LENGTH);
#endif
                    log_important("fatal_short1_send");
                    f_type = SEND_BROKEN;
                    rvalue = n - ID_LENGTH;
                    //                return n-39;
                }
                else if ((n < ID_LENGTH) && (n > 0))
                {
#ifdef DEBUG
                    sprintf(log_text, "%sfinal err:%d\tsend short2 error\tresult:%ld\n", log_text, tmp_erro, -1L);
#endif
                    log_important("fatal_short2_send");
                    f_type = SEND_FAIL;
                    rvalue = -1;
                }
                else
                {
#ifdef DEBUG
                    sprintf(log_text, "%sfinal err:%d\tsend short3 error\tresult:%ld\n", log_text, tmp_erro, 0L);
#endif
                    log_important("note_short3_send");
                    f_type = SEND_OTHER;
                    rvalue = 0;
                }
            }
#ifdef DEBUG
            log_event(log_text);
#endif
            tmp_erro = errno;
            errno = 0;
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), tmp_id, gettime(), F_SEND, n, len, rvalue, sockfd, f_type, buf);
            errno = tmp_erro;
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,remote_traced=%d,len=%d,n=%d,buf=", __LINE__, __func__, remote_traced, len, n);
                log_important(tmp);
                // log_important(buf);
            }
            return rvalue;
        }
        else
        {
            // the remote is not traced
            /*             if (log_error_info)
                        {
                            sprintf(tmp, "Log,line=%d,func=%s,sockfd=%d,len=%d", __LINE__, __func__, sockfd, len);
                            log_important(tmp);
                        } */
            n = old_send(sockfd, buf, len, flags);
            tmp_erro = errno;
            errno = 0;
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_SEND, n, len, n, sockfd, SEND_REG_REMOTE_NOT_TRACED, buf);
            errno = tmp_erro;
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,remote_traced=%d,len=%d,n=%d,buf=", __LINE__, __func__, remote_traced, len, n);
                log_important(tmp);
                // log_important(buf);
            }
            return n;
        }

    } // AF_INET

    return old_send(sockfd, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    init_context("sendto");
    static void *handle = NULL;
    static SENDTO old_sendto = NULL;
    ;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_sendto = (SENDTO)dlsym(handle, "sendto");
    }
    if (!is_socket(sockfd))
    {
        return old_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    }
    log_important("in sendto");
    // check socket type first
    sa_family_t socket_family = get_socket_family(sockfd);
    if (socket_family == AF_INET)
    {
        int tmp_erro = 0;
        struct sockaddr_in sin; // local socket info
        struct sockaddr_in son; // remote socket into
        socklen_t s_len = sizeof(sin);
        unsigned short int in_port = 0;
        unsigned short int on_port = 0;
        char in_ip[256] = "0.0.0.0";
        char on_ip[256] = "0.0.0.0";
        if (getsockname(sockfd, (struct sockaddr *)&sin, &s_len) != -1)
        {
            char *tmp_in_ip;
            in_port = ntohs(sin.sin_port);
            tmp_in_ip = inet_ntoa(sin.sin_addr);
            memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getsock_send");
        }

        // no connection, actually for recv, no connection now already means error
        if (getpeername(sockfd, (struct sockaddr *)&son, &s_len) != -1)
        {
            char *tmp_on_ip;
            on_port = ntohs(son.sin_port);
            tmp_on_ip = inet_ntoa(son.sin_addr);
            memmove(on_ip, tmp_on_ip, strlen(tmp_on_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getpeer_send");
        }
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,errno=%d,on_ip=%s, on_port=%d,in_ip=%s,in_port=%d, sockfd=%d,  flags=%d", __LINE__, __func__, errno, on_ip, on_port, in_ip, in_port, sockfd, flags);
            log_important(tmp);
        }
    }
    return old_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    init_context("sendmsg");
    ;
    static void *handle = NULL;
    static SENDMSG old_sendmsg = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_sendmsg = (SENDMSG)dlsym(handle, "sendmsg");
    }

#ifdef DEBUG
    char log_text[LOG_LENGTH] = "init";
    sprintf(log_text, "%s\t", "in sendmsg");
#endif
    // check socket type first
    sa_family_t socket_family = get_socket_family(sockfd);
    if (socket_family == AF_INET)
    {
#ifdef DEBUG
        sprintf(log_text, "%s%s\t", log_text, "AF_INET socket");
#endif
        int tmp_erro = 0;
        struct sockaddr_in sin; // local socket info
        struct sockaddr_in son; // remote socket into
        socklen_t s_len = sizeof(sin);
        unsigned short int in_port = 0;
        unsigned short int on_port = 0;
        char in_ip[256] = "0.0.0.0";
        char on_ip[256] = "0.0.0.0";
        if (getsockname(sockfd, (struct sockaddr *)&sin, &s_len) != -1)
        {
            char *tmp_in_ip;
            in_port = ntohs(sin.sin_port);
            tmp_in_ip = inet_ntoa(sin.sin_addr);
            memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getsock_sendmsg");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getsock_sendmsg");
#endif
        }

        // no connection, actually for recv, no connection now already means error
        if (getpeername(sockfd, (struct sockaddr *)&son, &s_len) != -1)
        {
            char *tmp_on_ip;
            on_port = ntohs(son.sin_port);
            tmp_on_ip = inet_ntoa(son.sin_addr);
            memmove(on_ip, tmp_on_ip, strlen(tmp_on_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getpeer_sendmsg");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getpeer_sendmsg");
#endif
        }

#ifdef IP_PORT
        sprintf(log_text, "%swrite to %s:%d with %s:%d\t", log_text, on_ip, on_port, in_ip, in_port);
#endif

        // actually this information is currently unused
#ifdef DATA_INFO
        // get socket buff size
        int recv_size = 0; /* socket接收缓冲区大小 */
        int send_size = 0;
        socklen_t optlen;
        optlen = sizeof(send_size);
        if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket send buff failed!");
        }
        optlen = sizeof(recv_size);
        if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket recv buff failed!");
        }
#endif
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,on_ip=%s, on_port=%d,in_ip=%s,in_port=%d, errno=%d,sockfd=%d", __LINE__, __func__, on_ip, on_port, in_ip, in_port, errno, sockfd);
            log_important(tmp);
        }
        int local_should_be_traced = check_if_local_should_be_traced(in_ip, in_port);
        if (!local_should_be_traced)
        {
            return old_sendmsg(sockfd, msg, flags);
        }

        size_t total = 0;
        int i = 0;
        for (; i < msg->msg_iovlen; i++)
        {
            total += msg->msg_iov[i].iov_len;
        }
        size_t new_total = total + ID_LENGTH;
        ssize_t n = 0;
#ifdef DEBUG
        sprintf(log_text, "%ssupposed sendmsg data length:%zu\tnew data length:%lu\tsendmsg process:%u\t thread:%lu\t socketid:%d\t", log_text, total, new_total, getpid(), pthread_self(), sockfd);
#endif

        int remote_traced = query_reg_remote_peer_traced(in_ip, in_port, on_ip, on_port);
        if (with_uuid == 0 || remote_traced == FALSE)
        {
            n = old_sendmsg(sockfd, msg, flags);
            tmp_erro = errno;
#ifdef DEBUG
            sprintf(log_text, "%sresult:%ld\t%s\n", log_text, n, "sengmsg done without uuid!");
            log_event(log_text);
#endif
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_SENDMSG, n, total, n, sockfd, SEND_NORMALLY, NULL);
            errno = tmp_erro;
            return n;
        }
        else
        {
            char buf[new_total];
            size_t already = 0;

            char id[ID_LENGTH];
            random_uuid(id, total);
            char tmp_id[ID_LENGTH];
            format_uuid(id, tmp_id);

            struct iovec test;
            test.iov_base = buf;
            test.iov_len = new_total;
            struct msghdr mymsg;
            mymsg.msg_name = msg->msg_name;
            mymsg.msg_namelen = msg->msg_namelen;
            mymsg.msg_iov = &test;
            mymsg.msg_iovlen = 1;
            mymsg.msg_control = msg->msg_control;
            mymsg.msg_controllen = msg->msg_controllen;
            mymsg.msg_flags = msg->msg_flags;

            if (mymsg.msg_controllen != 0)
            {
                log_important("fatal_control_sendmsg");
            }

            memmove(&buf[already], id, ID_LENGTH);
            already += ID_LENGTH;
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,n=%d,remote_traced=%d,on_port=%d,total=%d", __LINE__, __func__, n, remote_traced, on_port, total);
                log_important(tmp);
            }
            int ori_iovlen = msg->msg_iovlen;
            struct iovec *ori_iov = msg->msg_iov;
            int j = 0;
            for (; j < ori_iovlen; j++)
            {
                memmove(&buf[already], ori_iov[j].iov_base, ori_iov[j].iov_len);
                already += ori_iov[j].iov_len;
            }

            n = old_sendmsg(sockfd, &mymsg, flags);
            tmp_erro = errno;
            errno = 0;

            while ((n != new_total) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
            {

                ssize_t tmpValue = 0;
                if (n != -1)
                {
                    test.iov_base = &buf[n];
                    test.iov_len = new_total - n;
                    tmpValue = old_sendmsg(sockfd, &mymsg, flags);
                }
                else
                {
                    tmpValue = old_sendmsg(sockfd, &mymsg, flags);
                }
                tmp_erro = errno;
                char tmp[LOG_LENGTH];
                sprintf(tmp, "sendmsg\tn:%ld\terro:%d\ttmp:%ld\n", n, tmp_erro, tmpValue);
                log_event(tmp);

                if (tmpValue >= 0)
                {
                    if (n > 0)
                    {
                        n += tmpValue;
                    }
                    else
                    {
                        n = tmpValue;
                    }
                }
            }

#ifdef DEBUG

            sprintf(log_text, "%s\tuuid:%s\tarray:%d\ttotal:%lu\treal sendmsg:%ld\treturn:%ld\t", log_text, tmp_id, mymsg.msg_iovlen, total, n, n - ID_LENGTH);
#endif
            if (n != new_total)
            {
                // should deal with this situation
#ifdef DEBUG
                sprintf(log_text, "%s\tfailed!\n", log_text);
                log_event(log_text);
#endif
                log_important("fatal_short_sendmsg");
                push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), tmp_id, gettime(), F_SENDMSG, n, total, n, sockfd, SEND_FAIL, NULL);
                errno = tmp_erro;

                return n;
            }
            else
            {
#ifdef DEBUG
                sprintf(log_text, "%s\tsuccess!\n", log_text);
                log_event(log_text);
#endif
                push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), tmp_id, gettime(), F_SENDMSG, n, total, total, sockfd, SEND_ID, NULL);

                errno = tmp_erro;
                return total;
            }
        }
    }

    return old_sendmsg(sockfd, msg, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    init_context("recvfrom");
    static void *handle = NULL;
    static RECVFROM old_recvfrom = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_recvfrom = (RECVFROM)dlsym(handle, "recvfrom");
    }

    return old_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
    init_context("sendfile");
    static void *handle = NULL;
    static SENDFILE old_sendfile = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_sendfile = (SENDFILE)dlsym(handle, "sendfile");
    }

    if (is_socket(out_fd))
    {
        log_important("in sendfile");
    }
    return old_sendfile(out_fd, in_fd, offset, count);
}

ssize_t sendfile64(int out_fd, int in_fd, off64_t *offset, size_t count)
{
    init_context("sendfile64");
    static void *handle = NULL;
    static SENDFILE64 old_sendfile64 = NULL;
    static WRITE old_write = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_sendfile64 = (SENDFILE64)dlsym(handle, "sendfile64");
        old_write = (WRITE)dlsym(handle, "write");
    }
    return old_sendfile64(out_fd, in_fd, offset, count);

    if (is_socket(in_fd))
    {
        log_important("note_insock_sendfile64");
    }
    if (!is_socket(out_fd))
    {
        return old_sendfile64(out_fd, in_fd, offset, count);
    }
#ifdef DEBUG
    char log_text[LOG_LENGTH] = "init";
    sprintf(log_text, "%s\t", "in sendfile64");
#endif
    sa_family_t socket_family = get_socket_family(out_fd);
    if (socket_family == AF_INET)
    {
#ifdef DEBUG
        sprintf(log_text, "%s%s\t", log_text, "AF_INET socket");
#endif
        int tmp_erro = 0;
        struct sockaddr_in sin; // local socket info
        struct sockaddr_in son; // remote socket into
        socklen_t s_len = sizeof(sin);
        unsigned short int in_port = 0;
        unsigned short int on_port = 0;
        char in_ip[256] = "0.0.0.0";
        char on_ip[256] = "0.0.0.0";
        if (getsockname(out_fd, (struct sockaddr *)&sin, &s_len) != -1)
        {
            char *tmp_in_ip;
            in_port = ntohs(sin.sin_port);
            tmp_in_ip = inet_ntoa(sin.sin_addr);
            memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getsock_write");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getsock_write");
#endif
        }

        // no connection, actually for recv, no connection now already means error
        if (getpeername(out_fd, (struct sockaddr *)&son, &s_len) != -1)
        {
            char *tmp_on_ip;
            on_port = ntohs(son.sin_port);
            tmp_on_ip = inet_ntoa(son.sin_addr);
            memmove(on_ip, tmp_on_ip, strlen(tmp_on_ip) + 1);
        }
        else
        {
            errno = 0;
            log_important("fatal_getpeer_write");
#ifdef DEBUG
            sprintf(log_text, "%s%s\t", log_text, "fatal_getpeer_write");
#endif
        }

#ifdef IP_PORT
        sprintf(log_text, "%swrite to %s:%d with %s:%d\t", log_text, on_ip, on_port, in_ip, in_port);
#endif

        // actually this information is currently unused
#ifdef DATA_INFO
        // get socket buff size
        int recv_size = 0; /* socket接收缓冲区大小 */
        int send_size = 0;
        socklen_t optlen;
        optlen = sizeof(send_size);
        if (getsockopt(out_fd, SOL_SOCKET, SO_SNDBUF, &send_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket send buff failed!");
        }
        optlen = sizeof(recv_size);
        if (getsockopt(out_fd, SOL_SOCKET, SO_RCVBUF, &recv_size, &optlen) != 0)
        {
            sprintf(log_text, "%s%s\t", log_text, "get socket recv buff failed!");
        }
#endif
#ifdef DEBUG
        sprintf(log_text, "%ssupposed sendfile64 data length:%zu\tnew data length:%lu\tsendfile64 process:%u\t thread:%lu\t socketid:%d\tfilefd:%d\t", log_text, count, count + ID_LENGTH, getpid(), pthread_self(), out_fd, in_fd);
#endif
        ssize_t n = 0;

        if (with_uuid == 0)
        { // no uuid, write this message

            n = old_sendfile64(out_fd, in_fd, offset, count);
            tmp_erro = errno;
#ifdef DEBUG
            sprintf(log_text, "%sresult:%ld\t%s\n", log_text, n, "sendfile done without uuid!");
            log_event(log_text);
#endif
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_SEND64, n, count, n, out_fd, SEND_NORMALLY, NULL);
            errno = tmp_erro;
            return n;
        }
        char id[ID_LENGTH];
        random_uuid(id, count);

        int flag = 0;
        long long ttime;
        ttime = gettime();
        n = old_write(out_fd, id, ID_LENGTH);
        tmp_erro = errno;
#ifdef DEBUG
        sprintf(log_text, "%sfirst err:%d\tfirst write:%ld\t", log_text, tmp_erro, n);
#endif
        if (n == ID_LENGTH)
        {
            flag = 1;
        }
        else if (n == 0)
        {
            flag = 0;
        }
        else
        {
            tmp_erro = errno;
            while ((n != ID_LENGTH) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
            {
                ssize_t tmpValue = 0;
                if (n != -1)
                {
                    tmpValue = old_write(out_fd, &id[n], ID_LENGTH - n);
                }
                else
                {
                    tmpValue = old_write(out_fd, &id[0], ID_LENGTH);
                }
                tmp_erro = errno;
                if (tmpValue >= 0)
                {
                    n += tmpValue;
                }
            }
            if (n == ID_LENGTH)
            {
                flag = 1;
            }
            else if (n == -1)
            {
                flag = 0;
            }
            else if ((n <= ID_LENGTH) && (n >= 1))
            {
                flag = 0;
                log_important("fatal_write_short_sendfile64");
            }
            else
            {
                flag = 0;
                log_important("fatal_write_noconnection_sendfile64");
            }
        }

        if (flag == 0)
        {
#ifdef DEBUG
            sprintf(log_text, "%sid_flag:%d\terrno:%d\tlength:%ld\tsendfile failed!", log_text, flag, tmp_erro, n);
            log_event(log_text);
#endif
            push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), "", gettime(), F_SEND64, n, count, n, out_fd, SEND_ERROR, NULL);
            errno = tmp_erro;
            return n;
        }

        // now start to send real data
        char tmp_id[ID_LENGTH];
        format_uuid(id, tmp_id);
        char f_type = -1;
        errno = 0;
        n = old_sendfile64(out_fd, in_fd, offset, count);
        tmp_erro = errno;
#ifdef DEBUG
        sprintf(log_text, "%sfirst err:%d\tfirst sendfile64:%ld\t", log_text, tmp_erro, n);
#endif
        ssize_t rvalue = 0;
        if (n == count)
        {
            f_type = SEND_ID;
            rvalue = count;
        }
        else if (n == 0)
        {
            f_type = SEND_DISCONN;
            rvalue = 0;
        }
        else
        {
            while ((n != count) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
            {
                ssize_t tmpValue = 0;
                if (n != -1)
                {
                    tmpValue = old_sendfile64(out_fd, in_fd, offset, count - n); // suppose senfile will set offset automatically. autually we should check whether it's null first
                }
                else
                {
                    tmpValue = old_sendfile64(out_fd, in_fd, offset, count);
                }
                log_important("note_offset_sendfile64");
                ssize_t
                    tmp_erro = errno;
                if (tmpValue >= 0)
                {
                    n += tmpValue;
                }
            }
            if (n == count)
            {
                f_type = SEND_ID;
                rvalue = count;
            }
            else if (n == -1)
            {
                f_type = SEND_ERROR;
                rvalue = -1;
                log_important("fatal_short1_sendfile64");
            }
            else if ((n <= count) && (n >= 1))
            {
                f_type = SEND_BROKEN;
                rvalue = -1;
                log_important("fatal_short2_sendfile64"); // should deal with this like how we deal with unfinished recv
                //                return n-39;
            }
            else
            {
                f_type = SEND_OTHER;
                rvalue = -1;
                log_important("fatal_short2_sendfile64"); // should deal with this like how we deal with unfinished recv
            }
        }
#ifdef DEBUG
        sprintf(log_text, "%suuid:%s\terrno:%d\tn:%ld\treturn:%ld\n", log_text, tmp_id, tmp_erro, n, rvalue);
        log_event(log_text);
#endif
        push_to_database(on_ip, on_port, in_ip, in_port, getpid(), pthread_self(), tmp_id, gettime(), F_SEND64, n, count, rvalue, out_fd, f_type, NULL);
        if (n != count)
        {
            errno = tmp_erro;
        }
        return rvalue;
    }

    return old_sendfile64(out_fd, in_fd, offset, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    init_context("readv");
    static void *handle = NULL;
    static READV old_readv = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_readv = (READV)dlsym(handle, "readv");
    }

    if (is_socket(fd))
    {
    }
    log_important("in readv\n");
    return old_readv(fd, iov, iovcnt);
}

void *intermedia(void *arg)
{

    struct thread_param *temp;

    void *(*start_routine)(void *);
    temp = (struct thread_param *)arg;

    pthread_setspecific(uuid_key, (void *)temp->uuid);

    int tmp_erro = errno;
    long int ktid = syscall(SYS_gettid);
    push_thread_db(getpid(), ktid, pthread_self(), temp->ppid, temp->pktid, temp->ptid, temp->ttime);
    errno = tmp_erro;

    void *tmp = temp->start_routine(temp->args);
    free(temp);
    return tmp;
}

pid_t vfork(void)
{
    init_context("vfork");
    static void *handle = NULL;
    static FORK old_fork = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_fork = (FORK)dlsym(handle, "fork");
    }

    char *uuid = (char *)pthread_getspecific(uuid_key);
    char parameter[1024] = "";
    sprintf(parameter, "ppid=%d&pktid=%ld&ptid=%lu&ttime=%lld&rtype=%d&unit_uuid=%s&", getpid(), syscall(SYS_gettid), pthread_self(), gettime(), R_THREAD, uuid);

    pid_t result = old_fork();
    if (result == 0)
    {
        sprintf(parameter, "%spid=%d&ktid=%ld&tid=%lu", parameter, getpid(), syscall(SYS_gettid), pthread_self());
        pthread_setspecific(uuid_key, (void *)uuid);
        push_thread_db2(parameter);
    }
    return result;
}

int execl(const char *path, const char *arg, ...)
{

    log_important("in execl");
    return 1;
}

int execlp(const char *file, const char *arg, ...)
{

    log_important("in execlp");
    return 1;
}

int execvp(const char *file, char *const argv[])
{
    init_context("execvp");
    char *unit_uuid = (char *)pthread_getspecific(uuid_key);
    setenv(idenv, unit_uuid, 1);
    static void *handle = NULL;
    static EXECVP old_execvp = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_execvp = (EXECVP)dlsym(handle, "execvp");
    }
    return old_execvp(file, argv);
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{

    static void *handle = NULL;
    static EXECVPE old_execvpe = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_execvpe = (EXECVPE)dlsym(handle, "execvpe");
    }
    log_important("in execvpe");
    return old_execvpe(file, argv, envp);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t length)
{

    // printf("connect");
    ;

    init_context("connect");

    static void *handle = NULL;
    static CONN old_conn = NULL;

    if (!handle)
    {

        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_conn = (CONN)dlsym(handle, "connect");
    }

    int result = old_conn(sockfd, addr, length);
    int tmp_erro = errno;
    struct sockaddr_in sin; // local socket info
    struct sockaddr_in son; // remote socket into
    socklen_t s_len = sizeof(sin);
    unsigned short int in_port = 0;
    unsigned short int on_port = 0;
    char in_ip[256] = "0.0.0.0";
    char on_ip[256] = "0.0.0.0";
    if (getsockname(sockfd, (struct sockaddr *)&sin, &s_len) != -1)
    {
        char *tmp_in_ip;
        in_port = ntohs(sin.sin_port);
        tmp_in_ip = inet_ntoa(sin.sin_addr);
        memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
    }
    if (getpeername(sockfd, (struct sockaddr *)&son, &s_len) != -1)
    {
        char *tmp_on_ip;
        on_port = ntohs(son.sin_port);
        tmp_on_ip = inet_ntoa(son.sin_addr);
        memmove(on_ip, tmp_on_ip, strlen(tmp_on_ip) + 1);
    }

    is_client_connect = TRUE;
    client_connect_sockfd = sockfd;

    TcpTuple *tcpTuple = (TcpTuple *)malloc(sizeof(TcpTuple));
    tcpTuple->flag = REG_REGISTER;
    strcpy(tcpTuple->ip, in_ip);
    tcpTuple->port = in_port;
    int regres = reg_central(tcpTuple);
    if (regres < 0)
    {
        log_important("Fatal central register. \n");
    }
    free(tcpTuple);

    push_event_to_database(F_CONNECT, result, gettime(), getpid(), syscall(SYS_gettid), pthread_self());

    errno = tmp_erro;
    return result;
}

int listen(int sockfd, int n)
{
    init_context("listen");
    static void *handle = NULL;
    static LISTEN old_listen = NULL;
    if (!handle)
    {

        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_listen = (LISTEN)dlsym(handle, "listen");
    }

    int tmp_erro = errno;
    struct sockaddr_in sin; // local socket info
    struct sockaddr_in son; // remote socket into
    socklen_t s_len = sizeof(sin);
    unsigned short int in_port = 0;
    unsigned short int on_port = 0;
    char in_ip[256] = "0.0.0.0";
    char on_ip[256] = "0.0.0.0";
    if (getsockname(sockfd, (struct sockaddr *)&sin, &s_len) != -1)
    {
        char *tmp_in_ip;
        in_port = ntohs(sin.sin_port);
        tmp_in_ip = inet_ntoa(sin.sin_addr);
        memmove(in_ip, tmp_in_ip, strlen(tmp_in_ip) + 1);
    }

    is_server_listen = TRUE;
    server_listen_sockfd = sockfd;

    // listen all ip, register all
    if (strcmp(in_ip, "0.0.0.0") == 0)
    {
        struct ifaddrs *ifAddrStruct = NULL;
        void *tmpAddrPtr = NULL;
        getifaddrs(&ifAddrStruct);
        while (ifAddrStruct != NULL)
        {
            if (ifAddrStruct->ifa_addr->sa_family == AF_INET)
            {
                // check it is IP4            // is a valid IP4 Address
                tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                sprintf(tmp, "%s IP Address %s\n", ifAddrStruct->ifa_name, addressBuffer);
                log_important(tmp);
                TcpTuple *tcpTuple = (TcpTuple *)malloc(sizeof(TcpTuple));
                tcpTuple->flag = REG_REGISTER;
                strcpy(tcpTuple->ip, addressBuffer);
                tcpTuple->port = in_port;
                int regres = reg_central(tcpTuple);
                if (regres < 0)
                {
                    log_important("Fatal central register. \n");
                }
                free(tcpTuple);
            }
            ifAddrStruct = ifAddrStruct->ifa_next;
        }
    }
    else
    {
        TcpTuple *tcpTuple = (TcpTuple *)malloc(sizeof(TcpTuple));
        tcpTuple->flag = REG_REGISTER;
        strcpy(tcpTuple->ip, in_ip);
        tcpTuple->port = in_port;
        int regres = reg_central(tcpTuple);
        if (regres < 0)
        {
            log_important("Fatal central register. \n");
        }
        free(tcpTuple);
    }

    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,in_ip=%s,in_port=%d", __LINE__, __func__, in_ip, in_port);
        log_important(tmp);
    }

    int result = old_listen(sockfd, n);
    push_event_to_database(F_LISTEN, result, gettime(), getpid(), syscall(SYS_gettid), pthread_self());

    errno = tmp_erro;
    return result;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    init_context("accept");
    static void *handle = NULL;
    static ACCEPT old_accept = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_accept = (ACCEPT)dlsym(handle, "accept");
    }

    int tmp_erro = errno;

    int result = old_accept(sockfd, addr, addrlen);
    push_event_to_database(F_ACCEPT, result, gettime(), getpid(), syscall(SYS_gettid), pthread_self());
    errno = tmp_erro;

    return result;
}

// result in *** stack smashing detected ***: ssh terminated
int close(int fd)
{
    init_context("close");
    static void *handle = NULL;
    static CLOSE old_close = NULL;
    struct sockaddr_in sin;
    struct sockaddr_in son;
    socklen_t s_len = sizeof(sin);
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_close = (CLOSE)dlsym(handle, "close");
    }

    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s, fd=%d", __LINE__, __func__, fd);
        log_important(tmp);
    }
    // return old_close(fd);
    //  check socket type first
    sa_family_t socket_family = get_socket_family(fd);
    if (socket_family == AF_INET)
    {
        if ((getsockname(fd, (struct sockaddr *)&sin, &s_len) != -1) && (getpeername(fd, (struct sockaddr *)&son, &s_len) != -1))
        {
            unsigned short int in_port;
            unsigned short int on_port;
            char *in_ip;
            char *on_ip;
            in_port = ntohs(sin.sin_port);
            in_ip = inet_ntoa(sin.sin_addr);
            on_port = ntohs(son.sin_port);
            on_ip = inet_ntoa(son.sin_addr);

            // only an index, flag has no real meaning
            quadruple *q = get_init_quadruple(3, in_ip, in_port, on_ip, on_port);
            TcpTuple *t = (TcpTuple *)malloc(sizeof(TcpTuple));
            t->flag = REG_DEREGISTER;
            strcpy(t->ip, in_ip);
            t->port = in_port;
            // handle three types of sockfd
            // close(b)
            if (is_client_connect && client_connect_sockfd == fd)
            {
                deregister_local_register(q);

                reg_central(t);
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,is_client_connect deregister %d,fd=%d", __LINE__, __func__, in_port, fd);
                    log_important(tmp);
                }
            }
            // close(a)
            if (is_server_listen && fd == server_listen_sockfd)
            {
                reg_central(t);
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,is_server_listen deregister %d,fd=%d", __LINE__, __func__, in_port, fd);
                    log_important(tmp);
                }
            }
            // close(c)
            if (is_server_accept && fd == server_accept_sockfd)
            {
                deregister_local_register(q);
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,is_server_accept deregister %d,fd=%d", __LINE__, __func__, in_port, fd);
                    log_important(tmp);
                }
            }
            int result = old_close(fd);
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,fd=%d", __LINE__, __func__, fd);
                log_important(tmp);
            }
            push_event_to_database(F_CLOSE, result, gettime(), getpid(), syscall(SYS_gettid), pthread_self());

            return result;
        }
    }
    return old_close(fd);
}

int get_filename(FILE *f, char *filename)
{
    int fd;
    char fd_path[255];
    //    char * filename = malloc(255);
    ssize_t n;

    fd = fileno(f);
    sprintf(fd_path, "/proc/self/fd/%d", fd);
    n = readlink(fd_path, filename, 255);
    if (n < 0)
        return 0;
    filename[n] = '\0';
    return 1;
}

int get_filename2(int fd, char *filename)
{
    char fd_path[255];
    //    char * filename = malloc(255);
    ssize_t n;

    sprintf(fd_path, "/proc/self/fd/%d", fd);
    n = readlink(fd_path, filename, 255);
    if (n < 0)
        return 0;
    filename[n] = '\0';
    return 1;
}

// wether a environment variable is null or unset
int is_null(char *env)
{
    if ((!env) || (strcmp(env, "(null)") == 0) || (strcmp(env, "null") == 0) || (env == "") || (env == "(null)") || (env == "null"))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int get_own_env(char *env)
{
    int i = 0;
    for (; environ[i] != NULL; i++)
    {
    }
    char *tmp = getenv(idenv);
    if (!tmp)
    {
        return 0;
    }
    else
    {
        memmove(env, tmp, ID_LENGTH);
        // unsetenv(idenv);
        return 1;
    }
}

// result in unknown fault, resource manager cannot start

sa_family_t get_socket_family(int sockfd)
{
    struct sockaddr_storage tt_in;
    struct sockaddr *t_in = (struct sockaddr *)&tt_in;
    socklen_t s_len = sizeof(tt_in);
    if (getsockname(sockfd, t_in, &s_len) != -1)
    {
        return t_in->sa_family;
    }
    else
    {
        return UNKNOWN_FAMILY;
    }
}

char **init_uuids(ssize_t m)
{
    char **a = (char **)malloc(10 * sizeof(char *));
    int i = 0;
    for (i = 0; i < 10; i++)
    {
        a[i] = malloc(ID_LENGTH);
    }
    return a;
}

void free_uuids(char **a)
{
    int i;
    for (i = 0; i < 10; i++)
    {
        free(a[i]);
    }
    free(a);
}

// retval: if S_MARK, 0 means success, -1 means failure;
//         if S_QUERY, 1 means remotetraced, 0 means not remotetraced, -1 means not found
ssize_t remotetraced_storage(int type, int sockfd, int remote_traced)
{
    static struct remotetraced_struct *buffer_remotetraced_storage[S_SIZE];
    static short initialized = 0;
    static long counter = 0;
    if (type == S_MARK)
    { // mark
        if (initialized == 0)
        {
            int i;
            for (i = 0; i < S_SIZE; i++)
            {
                buffer_remotetraced_storage[i] = malloc(sizeof(struct remotetraced_struct));
                buffer_remotetraced_storage[i]->sockfd = 0;
                buffer_remotetraced_storage[i]->used = FALSE;
                buffer_remotetraced_storage[i]->remotetraced = FALSE;
            }
            initialized = 1;
            counter += 20 * 13;
        }

        int i;
        for (i = 0; i < S_SIZE; i++)
        { // check if this sockfd is already marked; if it is marked, update its status
            if (buffer_remotetraced_storage[i]->used && (buffer_remotetraced_storage[i]->sockfd == sockfd))
            {
                buffer_remotetraced_storage[i]->remotetraced = remote_traced;
                return 0;
            }
        }
        // now this sockfd is not marked yet; mark it now
        for (i = 0; i < S_SIZE; i++)
        {
            if (!buffer_remotetraced_storage[i]->used)
            {
                buffer_remotetraced_storage[i]->sockfd = sockfd;
                buffer_remotetraced_storage[i]->used = TRUE;
                buffer_remotetraced_storage[i]->remotetraced = remote_traced;
                return 0;
            }
        }

        log_important("fatal_nospace_storage");
        return -1;
    }
    else if (type == S_QUERY)
    {
        if (initialized == 0)
        {
            log_important("initialized=0");
            return -1; // not found
        }
        int i;
        for (i = 0; i < S_SIZE; i++)
        {
            if (buffer_remotetraced_storage[i]->used && (buffer_remotetraced_storage[i]->sockfd == sockfd))
            {
                if (buffer_remotetraced_storage[i]->remotetraced)
                    return 1;
                return 0;
            }
        }
        return -1; // not found
    }
}

ssize_t op_storage(int type, int sockfd, size_t left)
{

    static struct storage *buffer_storage[S_SIZE];
    static short initialized = 0;
    static long counter = 0;

    if (type == S_PUT)
    { // put
        if (initialized == 0)
        {
            int i;
            for (i = 0; i < S_SIZE; i++)
            {
                buffer_storage[i] = malloc(sizeof(struct storage));
                buffer_storage[i]->sockfd = 0;
                buffer_storage[i]->used = 0;
                buffer_storage[i]->left = 0;
            }
            initialized = 1;
            counter += 20 * 13;
        }

        int i;
        for (i = 0; i < S_SIZE; i++)
        {
            if (buffer_storage[i]->used == 0)
            {
                buffer_storage[i]->used = 1;
                buffer_storage[i]->sockfd = sockfd;
                buffer_storage[i]->left = left;
                return 0;
            }
        }
        log_important("fatal_nospace_storage");
        return -1;
    }
    else if (type == S_GET)
    { // get
        if (initialized == 0)
        {
            return 0;
        }
        int i;
        for (i = 0; i < S_SIZE; i++)
        {
            if (buffer_storage[i]->sockfd == sockfd)
            {
                size_t result = buffer_storage[i]->left;
                buffer_storage[i]->sockfd = 0;
                buffer_storage[i]->used = 0;
                buffer_storage[i]->left = 0;
                return result;
            }
        }
        return 0;
    }
    else if (type == S_RELEASE)
    { // release
        int i;
        for (i = 0; i < S_SIZE; i++)
        {
            if (buffer_storage[i]->sockfd == sockfd)
            {
                buffer_storage[i]->used = 0;
                buffer_storage[i]->sockfd = 0;
                return 0;
            }
        }
        return 0;
    }
    else
    {
        return -1;
    }
}

int check_storage(char *buf)
{
    int len = (int)buf[0];
    int i = 0;
    int flag = 0;
    if (len == (ID_LENGTH - 1))
    {
        for (i = 3; i < (len - 1); i++)
        {
            if (((buf[i] >= 97) && (buf[i] <= 122)) || ((buf[i] >= 48) && (buf[i] <= 57)) || (buf[i] == '-'))
            {
                flag = 1;
            }
            else
            {
                flag = 0;
                break;
            }
        }
        if ((flag == 1) && (buf[1] == '@') && (buf[2] == '@') && (buf[len - 1] == '@') && (buf[len] == '@'))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if (len >= 2)
        {
            for (i = 3; i <= len; i++)
            {
                if (((buf[i] >= 97) && (buf[i] <= 122)) || ((buf[i] >= 48) && (buf[i] <= 57)) || (buf[i] == '-') || (buf[i] == '@'))
                {
                    flag = 1;
                }
                else
                {
                    flag = 0;
                    break;
                }
            }
            if ((flag == 1) && (buf[1] == '@') && (buf[2] == '@'))
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            if (buf[1] == '@')
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
}

void init_string(struct string *s)
{
    s->len = 0;
    s->ptr = malloc(s->len + 1);
    if (s->ptr == NULL)
    {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL)
    {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size * nmemb;
}

// write the message to the logfile
int getresponse(char *post_parameter)
{
    ;

    static void *handle = NULL;
    static FPRINTF old_fprintf = NULL;
    static FFLUSH old_fflush = NULL;
    static FILE *logFile = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_fprintf = (FPRINTF)dlsym(handle, "fprintf");
        old_fflush = (FFLUSH)dlsym(handle, "fflush");
    }
    int tmp_erro = errno;

    if (!logFile)
    { // now we open the permanent error-log file handle
        if ((logFile = fopen(dataFilePath, "a+")) == NULL)
        {
            printf("log_important: Cannot open file %s! errno=%d\n", dataFilePath, errno);
            errno = tmp_erro;
            return;
        }
        printf("dataFilePath opened, fileno=%d\n", fileno(logFile));
    }
    // now errLogFile is not NULL
    old_fprintf(logFile, "%s\n", post_parameter);

    old_fflush(logFile);
    errno = tmp_erro;
}

/**
 * Create random UUID
 *
 * @param buf - buffer to be filled with the uuid string
 */
char *random_uuid(char buf[ID_LENGTH], size_t len)
{

    buf = random_uuid1(buf);

    buf[ID_LENGTH - 1] = '\0';

    int i = 1;
    for (; i < (1 + sizeof(len)); i++)
    {
        buf[i] = '-';
    }

    buf[0] = '@';
    //    buf[1]=ID_LENGTH;//note. ID_LENGTH should be below 127
    if (len != 0)
    { // len=0 indicates it's for unit_id
        memmove(&buf[1], &len, sizeof(len));
    }
    buf[ID_LENGTH - 2] = '@';

    return buf;
}

int format_uuid(char uuid[ID_LENGTH], char format_uuid[ID_LENGTH])
{
    memmove(format_uuid, uuid, ID_LENGTH);
    size_t len = 0;
    int i = 1;
    for (; i < (1 + sizeof(len)); i++)
    {
        format_uuid[i] = '-';
    }
    return 1;
}

long long gettime()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return 1000000 * t.tv_sec + t.tv_usec;
}

// wether a file descriptor is a socket descriptor
int is_socket(int fd)
{
    struct stat statbuf;
    fstat(fd, &statbuf);
    if (S_ISSOCK(statbuf.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// wether a file descriptor is a regular file
int is_file(int fd)
{
    struct stat statbuf;
    fstat(fd, &statbuf);
    if (S_ISREG(statbuf.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// whether it's writing to a
int check_log(int fd, size_t count)
{
    struct stat statbuf;
    fstat(fd, &statbuf);
    if (S_ISREG(statbuf.st_mode))
    {
        char filename[1024] = "";
        get_filename2(fd, filename);
        if (strstr(filename, logRule) && (count > 1))
        { // found
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}
// only for importance information which usually indicates an error we didn't deal with

void log_important(char *info)
{
    static void *handle = NULL;
    static FPRINTF old_fprintf = NULL;
    static FFLUSH old_fflush = NULL;
    static FILE *errLogFile = NULL;
    // printf("log_important %s\n", info);

    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_fprintf = (FPRINTF)dlsym(handle, "fprintf");
        old_fflush = (FFLUSH)dlsym(handle, "fflush");
    }

    int tmp_erro = errno;
    if (!errLogFile)
    { // now we open the permanent error-log file handle
        if ((errLogFile = fopen(errFilePath, "a+")) == NULL)
        {
            errno = tmp_erro;
            return;
        }
    }

    old_fprintf(errLogFile, "%s\n", info);
    old_fflush(errLogFile);

    errno = tmp_erro;
}

void log_event(char *info)
{
    static void *handle = NULL;
    static FPRINTF old_fprintf = NULL;
    static FCLOSE old_fclose = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_fprintf = (FPRINTF)dlsym(handle, "fprintf");
        old_fclose = (FCLOSE)dlsym(handle, "fclose");
    }

    int tmp = errno;
    FILE *logFile;
    if ((logFile = fopen(filePath, "a+")) != NULL)
    {
        old_fprintf(logFile, "%s\n", info);
        old_fclose(logFile);
    }
    else
    {
        printf("log_event: Cannot open file %s!\n", filePath);
    }
    errno = tmp;
}

int log_message3(char message[], size_t length)
{
    if (length < 1)
    {
        return 0;
    }
    if (length > 1000)
    {
        return 0;
    }
    static void *handle = NULL;
    static FPRINTF old_fprintf = NULL;
    static FCLOSE old_close = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_fprintf = (FPRINTF)dlsym(handle, "fprintf");
        old_close = (FCLOSE)dlsym(handle, "close");
    }

    FILE *logFile;
    char result[LOG_LENGTH] = "";

    if ((logFile = fopen("/tmp/tracelog3.txt", "a+")) != NULL)
    {
        sprintf(result, "message:\tpid:%d\tktid:%ld\ttid:%lu\t", getpid(), syscall(SYS_gettid), pthread_self());

        int i = 0;
        for (; i < length; i++)
        {
            sprintf(result, "%s%c", result, message[i]);
            //            sprintf(result,"%s%d-",result,message[i]);
        }
        sprintf(result, "%s%c", result, '\0');
        old_fprintf(logFile, "%s\n", result);
        old_fclose(logFile);
    }
    else
    {
        printf("log_message3: Cannot open file %s!\n", "/tmp/tracelog3.txt");
    }
    //    errno=tmp;
    return 0;
}

void log_message(char message[], int length, const char *flag)
{
    if (length < 1)
    {
        return;
    }

    static void *handle = NULL;
    static FPRINTF old_fprintf = NULL;
    static FCLOSE old_fclose = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_fprintf = (FPRINTF)dlsym(handle, "fprintf");
        old_fclose = (FCLOSE)dlsym(handle, "fclose");
    }

    FILE *logFile;
    char result[LOG_LENGTH] = "";

    if ((logFile = fopen(debugFilePath, "a+")) != NULL)
    {
        sprintf(result, "message:\t%s\t", flag);

        int i = 0;
        //        if(length>ID_LENGTH){
        //            i=length-10; //only show the last ten char
        //        }
        for (; i < length; i++)
        {
            sprintf(result, "%s%c", result, message[i]);
            //            sprintf(result,"%s%d-",result,message[i]);
        }
        sprintf(result, "%s%c", result, '\0');
        old_fprintf(logFile, "%s\n", result);
        old_fclose(logFile);
    }
    else
    {
        printf("log_message: Cannot open file %s!\n", debugFilePath);
    }
    //    errno=tmp;
}

// return 1, this message contains a job_id
// return 0, this message doesn't contain a job_id
int find_job(char *content, const char *message, int length)
{

    int flag = 0;
    int i;
    for (i = 0; i < (length - 4); i++)
    {
        if (message[i] == 'j')
        {
            if ((message[i + 1] == 'o') && (message[i + 2] == 'b') && (message[i + 3] == '_'))
            {
                flag = 1;

                // add job_id prefix
                int prefix = 10;
                if (i < 10)
                {
                    prefix = i;
                }
                //                int j=i-prefix;
                int j = 0;
                for (; j < prefix; j++)
                {
                    // unprintable characters and ' " \n & '\'
                    if ((message[i - prefix + j] <= 31) || (message[i - prefix + j] == 34) || (message[i - prefix + j] == 39) || (message[i - prefix + j] == 38) || (message[i - prefix + j] == 92) || (message[i - prefix + j] == 127))
                    {
                        content[j] = '-';
                    }
                    else
                    {
                        content[j] = message[i - prefix + j];
                    }
                }
                // add prefix done

                // add job_id and suffix
                int suffix = 60;
                if ((length - i) < 60)
                {
                    suffix = length - i;
                }
                int k = 0;
                for (; k < suffix; k++)
                {
                    if ((message[i + k] <= 31) || (message[i + k] == 34) || (message[i + k] == 39) || (message[i + k] == 38) || (message[i + k] == 92) || (message[i + k] == 127))
                    {
                        content[j + k] = '-';
                    }
                    else
                    {
                        content[j + k] = message[i + k];
                    }
                }
                // add suffix done
                content[j + k] = '\0';
                // log_event(content);
                return flag;
            }
        }
    }
    return flag;
}

// check whether a message uses HTTP, we only parse response
HttpResponse *parseResponse(char *response)
{
    HttpResponse *_httpResponseTemp = (HttpResponse *)malloc(sizeof(HttpResponse));
    memset(_httpResponseTemp, 0, sizeof(HttpResponse));
    HttpResponse *_httpResponse = (HttpResponse *)malloc(sizeof(HttpResponse));
    memset(_httpResponse, 0, sizeof(HttpResponse));
    ;

    char *_start = response;
    // not HTTP response
    if (!(_start[0] == 'H' && _start[1] == 'T' && _start[2] == 'T' && _start[3] == 'P'))
    {
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s", __LINE__, __func__);
            log_important(tmp);
        }
        return NULL;
    }
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,_start[0] =%c", __LINE__, __func__, _start[0]);
        log_important(tmp);
    }
    // parse first line
    for (; *_start && *_start != '\r'; _start++)
    {
        if (_httpResponseTemp->version == NULL)
        {
            _httpResponseTemp->version = _start;
        }

        if (*_start == ' ')
        {
            if (_httpResponseTemp->statuscode == NULL)
            {
                _httpResponseTemp->statuscode = _start + 1;
            }
            else
            {
                _httpResponseTemp->desc = _start + 1;
            }
            *_start = '\0';
        }
    }
    *_start = '\0'; // \r -> \0
    _start++;       // skip \n
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,statuscode=%s", __LINE__, __func__, _httpResponseTemp->statuscode);
        log_important(tmp);
    }
    // parse header

    _start++;
    char *_line = _start;
    while (*_line != '\r' && *_line != '\0')
    {
        char *_key;
        char *_value;
        while (*(_start++) != ':')
            ;
        *(_start - 1) = '\0';
        _key = _line;
        _value = _start + 1;
        while (_start++, *_start != '\0' && *_start != '\r')
            ;
        *_start = '\0'; // \r -> \0
        _start++;       // skip \n

        _start++;
        _line = _start;

        if (!strcmp(_key, "Content-Length"))
        {
            _httpResponseTemp->bodySize = atoi(_value);
        }
    }
    // prase body , if the last line is not NULL, there is data.
    if (*_line == '\r')
    {
        _line += 2;
        _httpResponseTemp->body = _line;
    }
    if (_httpResponseTemp->version != NULL)
    {
        _httpResponse->version = strdup(_httpResponseTemp->version);
    }
    if (_httpResponseTemp->statuscode != NULL)
    {
        _httpResponse->statuscode = strdup(_httpResponseTemp->statuscode);
    }
    if (_httpResponseTemp->desc != NULL)
    {
        _httpResponse->desc = strdup(_httpResponseTemp->desc);
    }
    if (_httpResponseTemp->body != NULL)
    {
        _httpResponse->body = strdup(_httpResponseTemp->body);
    }
    _httpResponse->bodySize = _httpResponseTemp->bodySize;
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,statuscode=%s", __LINE__, __func__, _httpResponseTemp->statuscode);
        log_important(tmp);
    }
    // Core dump error
    // releaseHttpResponse(_httpResponseTemp);
    //_httpResponseTemp = NULL;
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s", __LINE__, __func__);
        log_important(tmp);
    }
    return _httpResponse;
}

void releaseHttpResponse(HttpResponse *httpResponse)
{

    if (httpResponse == NULL)
    {
        return;
    }

    if (httpResponse->version != NULL)
    {

        free(httpResponse->version);

        httpResponse->version = NULL;
    }

    if (httpResponse->statuscode != NULL)
    {

        free(httpResponse->statuscode);

        httpResponse->statuscode = NULL;
    }
    if (httpResponse->desc != NULL)
    {
        free(httpResponse->desc);
        httpResponse->desc = NULL;
    }
    if (httpResponse->body != NULL)
    {
        free(httpResponse->body);
        httpResponse->body = NULL;
    }
    free(httpResponse);
    httpResponse = NULL;
}

void translate(char *content, const char *message, int length)
{
    if (length <= 0)
    {
        content[0] = '\0';
        return;
    }
    else
    {
        int flag = length;
        if (length > 1024)
        {
            flag = 1024;
        }
        int i = 0;
        for (; i < flag; i++)
        {
            if ((message[i] <= 0) || (message[i] == 34) || (message[i] == 39))
            {
                content[i] = '-';
            }
            else
            {
                content[i] = message[i];
            }
        }
        content[flag - 1] = '\0';
        return;
    }
}

// call this at the staring poing of every function
// to init the unit_id, not_traced_port_list
void init_context(char *func_type)
{
    char strbuf[100];
    int the_pid = getpid();
    sprintf(strbuf, "entered the hook, pid=%d, func_type=%s", the_pid, func_type);
    log_important(strbuf);
    if (shmid != -1)
    {
        regaddr = (quadruple *)shmat(shmid, NULL, 0);
    }
    if (regaddr == NULL)
    {
        initLocalRegister();
    }

    if (uuid_key == 12345)
    {

        // new or after exec
        pthread_key_create(&uuid_key, NULL); // actually we should release the pointer when the thread exits
        char *tmp_env = malloc(ID_LENGTH);

        if (!get_own_env(tmp_env))
        { // check from the environment virable list

            random_uuid(tmp_env, 0);
        }
        pthread_setspecific(uuid_key, tmp_env);
    }
    else
    { // the context already existed

        void *context = pthread_getspecific(uuid_key);
        if (!context)
        {

            char *tmp_env = malloc(ID_LENGTH);

            random_uuid(tmp_env, 0);

            pthread_setspecific(uuid_key, (void *)tmp_env);
        }
    }
}

// type=24
/*long length:packet actual length(maybe there is header)
long supposed_length:high layer wants
long rlength:packet origin length
*/
int push_to_database(char *on_ip, int on_port, char *in_ip, int in_port, pid_t pid, pthread_t tid, char *uuid, long long time, char ftype, long length, long supposed_length, long rlength, int socketid, char dtype, const char *message)
{

    char *original_unit = (char *)pthread_getspecific(uuid_key);

    char tmp_unit[ID_LENGTH];

    if ((ftype % 2) == 0)
    { // recv
        if (uuid != "")
        {
            char *unit_uuid = malloc(ID_LENGTH);
            random_uuid(unit_uuid, 0);

            pthread_setspecific(uuid_key, (void *)unit_uuid);
            memmove(tmp_unit, unit_uuid, ID_LENGTH);
        }
        else
        {
            memmove(tmp_unit, original_unit, ID_LENGTH);
        }
    }
    else
    { // send
        if (uuid != "")
        {
            char *unit_uuid = malloc(ID_LENGTH);
            memmove(unit_uuid, uuid, ID_LENGTH);
            pthread_setspecific(uuid_key, (void *)unit_uuid);
            memmove(tmp_unit, original_unit, ID_LENGTH);
        }
        else
        {
            memmove(tmp_unit, original_unit, ID_LENGTH);
        }
    }

    // To do
    // now we only parse HTTP
    char *tmpMsg = (char *)malloc(LOG_LENGTH);
    char *savedMsg = (char *)malloc(LOG_LENGTH);
    int http_response_status_code = -1;

    if ((ftype == F_SEND) || (ftype == F_WRITE) || (ftype == F_READ) || (ftype == F_RECV))
    {
        if (rlength > 0)
        {
            // save message
            if (rlength > LOG_LENGTH)
            {
                memcpy(tmpMsg, message, LOG_LENGTH);
                memcpy(savedMsg, message, LOG_LENGTH);
            }
            else
            {
                memcpy(tmpMsg, message, rlength);
                memcpy(savedMsg, message, rlength);
            }
            // handle HTTP
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,rlength=%d,tmpMsg=%s", __LINE__, __func__, rlength, tmpMsg);
                log_important(tmp);
            }
            HttpResponse *parsed_http_response = parseResponse(tmpMsg);

            if (parsed_http_response)
            {
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,rlength=%d,tmpMsg=%s,statuscode=%s", __LINE__, __func__, rlength, tmpMsg, parsed_http_response->statuscode);
                    log_important(tmp);
                }
                char *tmp_code = parsed_http_response->statuscode;
                http_response_status_code = atoi(tmp_code);
            }
        }
        else
        {
            tmpMsg[0] = '\0';
        }
    }
    else
    {
        tmpMsg[0] = '\0';
    }

    char *parameter_tmp = "on_ip=%s&on_port=%d&in_ip=%s&in_port=%ld&pid=%u&ktid=%ld&tid=%lu&uuid=%s&unit_uuid=%s&ttime=%lld&ftype=%d&length=%ld&supposed_length=%ld&rlength=%ld&rtype=%d&socket=%d&dtype=%d&http_response_status_code=%d&message=%s";

    char parameter[4096];
    sprintf(parameter, parameter_tmp, on_ip, on_port, in_ip, in_port, pid, syscall(SYS_gettid), tid, uuid, tmp_unit, time, ftype, length, supposed_length, rlength, R_DATABASE, socketid, dtype, http_response_status_code, savedMsg);

    free(tmpMsg);
    //    free(tmpMsg2);

    return getresponse(parameter);
    //    return 0;
}

// type=27
int push_thread_db(long int pid, long int ktid, pthread_t tid, long int ppid, long int pktid, pthread_t ptid, long long time)
{
    // To do
    int tmp_erro = errno;
    char *original_unit = (char *)pthread_getspecific(uuid_key);
    char tmp_unit[ID_LENGTH];
    memmove(tmp_unit, original_unit, ID_LENGTH);
    char *parameter_tmp = "pid=%ld&ktid=%ld&tid=%lu&ppid=%ld&pktid=%ld&ptid=%lu&ttime=%lld&rtype=%d&unit_uuid=%s";
    char parameter[2048];
    sprintf(parameter, parameter_tmp, pid, ktid, tid, ppid, pktid, ptid, time, R_THREAD, tmp_unit);

    return getresponse(parameter);

    //    return 0;
}
// type=28
int push_thread_dep(pid_t pid, pid_t ktid, pthread_t dtid, pthread_t jtid, long long time)
{
    char *original_unit = (char *)pthread_getspecific(uuid_key);
    char *parameter_tmp = "pid=%ld&ktid=%ld&dtid=%lu&jtid=%lu&ttime=%lld&rtype=%d&unit_uuid=%s";
    char parameter[2048];
    sprintf(parameter, parameter_tmp, pid, ktid, dtid, jtid, time, R_THREAD_DEP, original_unit);
    //    log_message(parameter,strlen(parameter));
    return getresponse(parameter);
    //    return 0;
}

// type=27
int push_thread_db2(char *parameter)
{

    return getresponse(parameter);
    //    return 0;
}

// type=29
int push_event_to_database(int event_type, int result, long long time, pid_t process, pid_t ktid, pthread_t tid)
{

    char *parameter_tmp = "ftype=%d&result=%d&ltime=%lld&pid=%u&ktid=%u&tid=%lu&unit_uuid=%s&rtype=%d";
    char *original_unit = (char *)pthread_getspecific(uuid_key);
    char parameter[2048];
    sprintf(parameter, parameter_tmp, event_type, result, time, process, ktid, tid, original_unit, R_EVENT);
    return getresponse(parameter);
}

ssize_t check_read_header(char *uuids, int sockfd, size_t *length, int flags)
{
    static void *handle = NULL;
    static RECV old_recv = NULL; // real recv
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_recv = (RECV)dlsym(handle, "recv");
    }
    //    ssize_t rvalue=0;
    ssize_t n = 0;
    int tmp_erro = 0;
    n = old_recv(sockfd, uuids, ID_LENGTH, flags);
    tmp_erro = errno;
    if ((n != -1) && (n != ID_LENGTH) && (n != 0))
    { // n<ID_LENGTH
        while ((n != ID_LENGTH) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
        {
            ssize_t tmpValue = 0;
            if (n == -1)
            {
                tmpValue = old_recv(sockfd, uuids, ID_LENGTH, flags);
            }
            else
            {
                tmpValue = old_recv(sockfd, &uuids[n], ID_LENGTH - n, flags);
            }
            tmp_erro = errno;

            if (tmpValue >= 0)
            {
                if (n > 0)
                {
                    n += tmpValue;
                }
                else
                {
                    n = tmpValue;
                }
            }
        }
    }

    if (n == ID_LENGTH)
    {

        if ((uuids[0] == '@') && (uuids[ID_LENGTH - 2] == '@') && (uuids[ID_LENGTH - 1] == '\0'))
        {
            //        uint32_t tmpValue=ntohl(*((uint32_t *)&uuids[1]));
            //        *length=tmpValue;
            *length = *((size_t *)&uuids[1]);
            int i = 0;
            for (; i < sizeof(length); i++)
            {
                uuids[i + 1] = '-'; // just for print
            }

            errno = 0;
            return ID_LENGTH;
        }
        else
        { // if it's not ID
            *length = 0;
            log_message(uuids, ID_LENGTH, "check1");
            errno = 0;
            return ID_LENGTH;
        }
    }
    else if (n == -1)
    {
        errno = tmp_erro;
        return -1;
    }
    else if ((n < ID_LENGTH) && (n > 0))
    {
        log_important("fatal_shorthead_readhead");
        log_message(uuids, n, "check2");
        errno = tmp_erro;
        return -1; // actually we should deal wth the rest of DATA_ID, and even if it's not a ID (from writev)
    }
    else
    {
        log_important("fatal_shorthead2_readhead");
        errno = tmp_erro;
        return 0;
    }
}

ssize_t check_recvmsg_rest(struct msghdr *msg, int sockfd, size_t length, size_t buf_len, int flags)
{
    static void *handle = NULL;
    static RECVMSG old_recvmsg = NULL;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_recvmsg = (RECVMSG)dlsym(handle, "recvmsg");
    }
    int tmp_erro = 0;
    struct iovec *ori_iov = msg->msg_iov;
    int ori_iovlen = msg->msg_iovlen;

    size_t limit = length;
    if (length > buf_len)
    {
        limit = buf_len;
    }

    struct iovec test;
    char buf[limit];
    test.iov_base = buf;
    test.iov_len = limit;
    msg->msg_iov = &test;
    msg->msg_iovlen = 1;

    ssize_t n = old_recvmsg(sockfd, msg, flags);
    tmp_erro = errno;

    while ((n != limit) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
    {
        ssize_t tmpValue = 0;
        if (n != -1)
        {
            test.iov_base = &buf[n];
            test.iov_len = limit - n;
            tmpValue = old_recvmsg(sockfd, msg, flags);
        }
        else
        {
            tmpValue = old_recvmsg(sockfd, msg, flags);
        }
        tmp_erro = errno;

        if (tmpValue >= 0)
        {
            if (n > 0)
            {
                n += tmpValue;
            }
            else
            {
                n = tmpValue;
            }
        }
    }

    if (n == limit)
    {
        if (limit == buf_len)
        {
            op_storage(S_PUT, sockfd, length - buf_len);
        }
        size_t left = limit;
        int j = 0;
        for (; j < ori_iovlen; j++)
        {
            if (left > ori_iov[j].iov_len)
            {
                memmove(ori_iov[j].iov_base, &buf[limit - left], ori_iov[j].iov_len);
                left = left - ori_iov[j].iov_len;
            }
            else
            {
                memmove(ori_iov[j].iov_base, &buf[limit - left], left);
                break;
            }
        }
        msg->msg_iov = ori_iov;
        msg->msg_iovlen = ori_iovlen;
        return limit;
    }
    else if (n == -1)
    {
        log_important("note_less0_readrecvrest");
        errno = tmp_erro;
        msg->msg_iov = ori_iov;
        msg->msg_iovlen = ori_iovlen;
        return -1;
    }
    else if (n == 0)
    {
        log_important("note_less1_readrecvrest");
        errno = tmp_erro;
        msg->msg_iov = ori_iov;
        msg->msg_iovlen = ori_iovlen;
        return 0;
    }
    else
    { // n>0 n<limit
        //        log_important("note_less2_readrecvrest");
        op_storage(S_PUT, sockfd, length - n);
        errno = tmp_erro;
        msg->msg_iov = ori_iov;
        msg->msg_iovlen = ori_iovlen;
        return n;
    }
}

ssize_t check_read_rest(char *buf, int sockfd, size_t length, size_t count, int flags)
{

    static void *handle = NULL;
    static RECV old_recv = NULL;
    ;
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_recv = (RECV)dlsym(handle, "recv");
    }
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,count=%d,length=%d", __LINE__, __func__, count, length);
        log_important(tmp);
    }
    int tmp_erro = 0;
    ssize_t result = 0;
    if (count < length)
    { // should mark as some bytes left
        result = old_recv(sockfd, buf, count, flags);
        tmp_erro = errno;
        while ((result != count) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
        {
            ssize_t tmpValue = 0;
            if (result != -1)
            {
                tmpValue = old_recv(sockfd, &buf[result], count - result, flags);
            }
            else
            {
                tmpValue = old_recv(sockfd, &buf[0], count, flags);
            }
            tmp_erro = errno;
            if (tmpValue > 0)
            {
                if (result > 0)
                {
                    result += tmpValue;
                }
                else
                {
                    result = tmpValue;
                }
            }
            if (tmpValue == 0)
            {
                break;
            }
        }
        if (result > 0)
        {
            op_storage(S_PUT, sockfd, length - result); // here it should length-result
        }

        if (result != count)
        {
            sprintf(tmp, "here:result:%ld\tcount:%lu\n", result, count);
            log_message(tmp, strlen(tmp), "read_rest");
            log_important("fatal_short_readrest");
        }
        return result;
    }
    else
    {
        result = old_recv(sockfd, buf, length, flags);

        while ((result != length) && ((tmp_erro == 0) || (tmp_erro == EAGAIN) || (tmp_erro == EWOULDBLOCK)))
        {

            ssize_t tmpValue = 0;

            if (result != -1)
            {
                tmpValue = old_recv(sockfd, &buf[result], length - result, flags);
            }
            else
            {
                tmpValue = old_recv(sockfd, &buf[0], length, flags);
            }
            tmp_erro = errno;
            if (tmpValue > 0)
            {
                if (result > 0)
                {
                    result += tmpValue;
                }
                else
                {
                    result = tmpValue;
                }
            }
            if (tmpValue == 0)
            {
                if (log_error_info)
                {
                    sprintf(tmp, "Log,line=%d,func=%s,result=%d,length=%d,tmp_errno=%d,tmpValue=%d", __LINE__, __func__, result, length, tmp_erro, tmpValue);
                    log_important(tmp);
                }
                break;
            }
        }
        if (result == length)
        {
            return result;
        }
        else if (result == -1)
        {
            log_important("note_less0_readrest");
            errno = tmp_erro;
            return -1;
        }
        else if (result == 0)
        {
            log_important("note_less1_readrest");
            return 0;
        }
        else
        {
            //            log_important("note_less2_readrest");
            op_storage(S_PUT, sockfd, length - result);
            errno = tmp_erro;
            return result;
        }
    }
}

char *random_uuid1(char buf[ID_LENGTH])
{
    const char *c = "89ab";
    char *p = buf;
    int n;
    for (n = 0; n < 16; ++n)
    {
        int b = rand() % 255;
        switch (n)
        {
        case 6:
            sprintf(p, "4%x", b % 15);
            break;
        case 8:
            sprintf(p, "%c%x", c[rand() % strlen(c)], b % 15);
            break;
        default:
            sprintf(p, "%02x", b);
            break;
        }

        p += 2;
        switch (n)
        {
        case 3:
        case 5:
        case 7:
        case 9:
            *p++ = '-';
            break;
        }
    }
    *p = 0;
    return buf;
}

// for share memory
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
void initLocalRegister()
{
    int shmid = getShm(sizeof(quadruple) * REGISTER_LENTH);
    if (shmid < 0)
    {
        sprintf(tmp, "errno=%d,strerror=%s\n", shmid, errno, strerror(errno));
    }
    sprintf(tmp, "shmid=%d\n", shmid);
    log_important(tmp);
    regaddr = (quadruple *)shmat(shmid, NULL, 0);

    if (regaddr[REGISTER_LENTH - 1].flag != REGISTER_INIT_FLAG)
    {
        printf("initial local register...\n");
        int i;
        for (i = 0; i < REGISTER_LENTH; i++)
        {
            regaddr[i].flag = 0;
            strcpy(regaddr[i].in_ip, "");
            regaddr[i].in_port = 0;
            strcpy(regaddr[i].on_ip, "");
            regaddr[i].on_port = 0;
        }

        regaddr[REGISTER_LENTH - 1].flag = REGISTER_INIT_FLAG;

        regaddr[1].flag = REGISTER_END;
    }
}
// return 1:equal
// return 0:unequal
int cmpQuadruple(quadruple *q1, quadruple *q2)
{
    int res = 0;
    if ((q1->in_port == q2->in_port) && (strcmp(q1->in_ip, q2->in_ip) == 0) && (q1->on_port == q2->on_port) && (strcmp(q1->on_ip, q2->on_ip) == 0))
    {
        res = 1;
    }
    return res;
}
// not exist : return -1; else return index
int reg_central(TcpTuple *regTcpTuple)
{
    // just for debug
    /*     return -1; */

    static void *handle = NULL;
    static SOCKET old_socket = NULL;
    static CONN old_conn = NULL;
    static SEND old_send = NULL;
    static RECV old_recv = NULL;
    static CLOSE old_close = NULL;
    if (!handle)
    {

        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_socket = (SOCKET)dlsym(handle, "socket");
        old_conn = (CONN)dlsym(handle, "connect");
        old_send = (SEND)dlsym(handle, "send");
        old_recv = (RECV)dlsym(handle, "recv");
        old_close = (CLOSE)dlsym(handle, "close");
    }
    int s;
    struct sockaddr_in server;
    int n = 0;
    int res = -1; // fail
    struct hostent *he;
    he = gethostbyname(REG_SERVER_ADDR);

    s = old_socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr = *((struct in_addr *)he->h_addr);
    server.sin_port = htons(REG_SERVER_PORT);
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,sockfd=%d,register_type=%d,ip=%s,port=%d", __LINE__, __func__, s, regTcpTuple->flag, regTcpTuple->ip, regTcpTuple->port);
        log_important(tmp);
    }
    int conres = old_conn(s, (struct sockaddr *)&server, sizeof(server));
    while (conres == -1)
    {
        conres = old_conn(s, (struct sockaddr *)&server, sizeof(server));
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,conres=%d", __LINE__, __func__, conres);
            log_important(tmp);
        }
    }

    int conerrno = errno;
    // memset(buff, 0, REG_BUFFLEN);
    int needSend = sizeof(TcpTuple);
    char *buff = (char *)malloc(needSend);
    memcpy(buff, regTcpTuple, needSend);

    int senres = old_send(s, (char *)buff, needSend, 0);

    int senderrno = errno;
    // memset(buff, 0, REG_BUFFLEN);
    int pos = 0;
    while (pos < sizeof(res))
    {
        n = old_recv(s, (char *)&res + pos, sizeof(res), 0);
        pos += n;
        if (n <= 0)
        {
            log_important("Fatal recv");
            break;
        }
    }
    int recverrno = errno;

    old_close(s);
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,res=%d", __LINE__, __func__, res);
        log_important(tmp);
    }
    if (n > 0)
    {
        return res;
    }
    else
    {
        return -1;
    }
}
// maybe we do not need this function
// register fail:return -1 or flag
// else return index
int register_local_register(quadruple *q)
{
    // just for debug
    /*     return -1; */
    // illegal register
    if ((q->flag != 1) && (q->flag != 2))
    {
        return -1;
    }
    int i;
    int query_res = query_local_register(q);
    // q not in local register
    if (query_res == -1)
    {
        for (i = 0; regaddr[i].flag != REGISTER_END; i++)
        {
            // there is a slot
            if (regaddr[i].flag == 0)
            {
                // register
                strcpy(regaddr[i].in_ip, q->in_ip);
                regaddr[i].in_port = q->in_port;
                strcpy(regaddr[i].on_ip, q->on_ip);
                regaddr[i].on_port = q->on_port;
                regaddr[i].flag = q->flag;

                if ((regaddr[i + 1].flag == REGISTER_END) && (regaddr[i + 2].flag == 0) && (regaddr[i + 3].flag == 0) && (regaddr[i + 4].flag == 0) && (regaddr[i + 5].flag == 0))
                {
                    // register in last slot
                    regaddr[i + 1].flag = 0;
                    regaddr[i + 2].flag = REGISTER_END;
                }
                return i;
            }
        }
    }
    else
    {
        return query_res;
    }

    return -1;
}

// Sequential Search
// not found: return -1
// else return flag
int query_local_register(quadruple *q)
{
    // just for debug
    /*     return -1; */
    int i;
    for (i = 0; regaddr[i].flag != REGISTER_END; i++)
    {
        if ((cmpQuadruple(q, &regaddr[i]) == 1) && (regaddr[i].flag != 0))
        {
            return regaddr[i].flag;
        }
    }
    return -1;
}

//
int deregister_local_register(quadruple *q)
{

    int index;
    for (index = 0; regaddr[index].flag != REGISTER_END; index++)
    {
        if ((cmpQuadruple(q, &regaddr[index]) == 1) && (regaddr[index].flag != 0))
        {
            regaddr[index].flag = 0;
            memset(regaddr[index].in_ip, 0x00, sizeof(regaddr[index].in_ip));
            regaddr[index].in_port = 0;
            memset(regaddr[index].on_ip, 0x00, sizeof(regaddr[index].on_ip));
            regaddr[index].on_port = 0;
            if ((regaddr[index + 1].flag == 0) && (regaddr[index + 2].flag == REGISTER_END) && (regaddr[index + 3].flag == 0) && (regaddr[index + 4].flag == 0) && (regaddr[index + 5].flag == 0))
            { // deregister last
                regaddr[index + 1].flag = REGISTER_END;
                regaddr[index + 2].flag = 0;
            }
            return index;
        }
    }
    return -1;
}

int check_if_handle_fragment(int sockfd)
{
    size_t left = op_storage(S_GET, sockfd, 0);
    if (left == 0)
        return 0; // 0 is also FALSE

    if (left > 0)
        return left;

    log_important("Error in check_if_handle_fragment()");
    return FALSE;
}

/*
  How to determine if the local process should be REPTraced:
  (1) if the local IP is not in the legal IP list, it should not be traced
  (2) if the local IP is in the legal IP list but the local port is in the illegal port list, it should not be traced
*/
int check_if_local_should_be_traced(char *ip, int port)
{
    int should_be_traced = FALSE;
    int i = 0;

    for (i = 0; i < traced_ip_list_length; i++)
    {
        if (strcmp(ip, traced_ip_list[i]) == 0)
        {
            should_be_traced = TRUE;
            break;
        }
    }

    if (should_be_traced)
    {
        for (i = 0; i < not_traced_port_list_length; i++)
        {
            if (port == not_traced_port_list[i])
            {
                should_be_traced = FALSE;
                break;
            }
        }
    }

    return should_be_traced;
}

int read_header(char *uuids, int sockfd, size_t *length, int flags, ssize_t *consumed_byte_number, int len, int *recv_times)
{

    int header_found = FALSE;

    static void *handle = NULL;
    static RECV old_recv = NULL; // real recv
    if (!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_recv = (RECV)dlsym(handle, "recv");
    };
    ssize_t n = 0;

    int tmp_errno = 0;

    if (len < ID_LENGTH)
    {
        n = old_recv(sockfd, uuids, len, flags);
        *recv_times++;
        if (uuids[0] == '@')
        {
            // There is a header
            tmp_errno = errno;
            if ((n != -1) && (n != ID_LENGTH) && (n != 0))
            { // n<ID_LENGTH

                ssize_t tmpValue = 0;
                while ((n != ID_LENGTH) && ((tmp_errno == 0) || (tmp_errno == EAGAIN) || (tmp_errno == EWOULDBLOCK)))
                {

                    tmpValue = old_recv(sockfd, &uuids[n], ID_LENGTH - n, flags);
                    *recv_times++;

                    tmp_errno = errno;

                    if (tmpValue > 0)
                    {
                        if (n > 0)
                        {
                            n += tmpValue;
                        }
                        else
                        {
                            n = tmpValue;
                        }
                    }
                    else if (tmpValue == 0)
                    {
                        log_important("Packet length less than 38 bytes!");
                        break;
                    }
                }
            }
            *length = *((size_t *)&uuids[1]);
            int i = 0;
            for (; i < sizeof(length); i++)
            {

                uuids[i + 1] = '-'; // just for print
            }
            header_found = TRUE;
        }

        //  ;
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,n=%d,len=%d", __LINE__, __func__, n, len);
            log_important(tmp);
        }
    }
    else
    {

        n = old_recv(sockfd, uuids, ID_LENGTH, flags);
        *recv_times++;
        tmp_errno = errno;
        if (log_error_info)
        {
            sprintf(tmp, "Log,line=%d,func=%s,n=%d,tmp_errno=%d,len=%d", __LINE__, __func__, n, tmp_errno, len);
            log_important(tmp);
        }
        if ((n != -1) && (n != ID_LENGTH) && (n != 0) && (uuids[0] == '@'))
        { // n<ID_LENGTH
            if (log_error_info)
            {
                sprintf(tmp, "Log,line=%d,func=%s,n=%d,tmp_errno=%d,len=%d", __LINE__, __func__, n, tmp_errno, len);
                log_important(tmp);
            }
            ssize_t tmpValue = 0;
            while ((n != ID_LENGTH) && ((tmp_errno == 0) || (tmp_errno == EAGAIN) || (tmp_errno == EWOULDBLOCK)))
            {
                tmpValue = old_recv(sockfd, &uuids[n], ID_LENGTH - n, flags);
                tmp_errno = errno;

                if (tmpValue > 0)
                {
                    if (n > 0)
                    {
                        n += tmpValue;
                    }
                    else
                    {
                        n = tmpValue;
                    }
                }
                else if (tmpValue == 0)
                {
                    log_important("Packet length less than 38 bytes!");
                    break;
                }
            }
        }
        if (n == ID_LENGTH)
        {
            if ((uuids[0] == '@') && (uuids[ID_LENGTH - 2] == '@') && (uuids[ID_LENGTH - 1] == '\0'))
            {
                *length = *((size_t *)&uuids[1]);
                int i = 0;
                for (; i < sizeof(length); i++)
                {

                    uuids[i + 1] = '-'; // just for print
                }
                header_found = TRUE;
            }
        }
    }

    if (!header_found)
    {

        log_important("header not found in read_header()");
    }

    // errno = tmp_errno;
    *consumed_byte_number = n;
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s, consumed_byte_number=%d,header_found=%d,len=%d,errno=%d,recv_times=%d", __LINE__, __func__, *consumed_byte_number, header_found, len, errno, *recv_times);
        log_important(tmp);
    }

    return header_found;
}

quadruple *get_init_quadruple(int flag, char in_ip[16], int in_port, char on_ip[16], int on_port)
{
    quadruple *my_quadruple = (quadruple *)malloc(sizeof(quadruple));
    // init my_quadruple

    my_quadruple->flag = flag;
    strcpy(my_quadruple->in_ip, in_ip);
    my_quadruple->in_port = in_port;
    strcpy(my_quadruple->on_ip, on_ip);
    my_quadruple->on_port = on_port;
    return my_quadruple;
}

int query_reg_remote_peer_traced(char in_ip[16], int in_port, char on_ip[16], int on_port)
{
    // query in local
    // only an index, flag has no real meaning
    quadruple *q = get_init_quadruple(1, in_ip, in_port, on_ip, on_port);

    int query_in_local_res = query_local_register(q);
    if (log_error_info)
    {
        sprintf(tmp, "Log,line=%d,func=%s,query_in_local_res=%d ", __LINE__, __func__, query_in_local_res);
        log_important(tmp);
    }
    int remote_traced = FALSE;
    // handle local register result
    if (query_in_local_res == 1)
    {
        remote_traced = TRUE;
    }
    else if (query_in_local_res == 2)
    {
        remote_traced = FALSE;
    }
    else
    {
        // not in local register, query central register
        TcpTuple *tcpTuple = (TcpTuple *)malloc(sizeof(TcpTuple));
        tcpTuple->flag = REG_QUERY;
        strcpy(tcpTuple->ip, on_ip);
        tcpTuple->port = on_port;
        int query_in_central_res = reg_central(tcpTuple);
        if (query_in_central_res == -1)
        {
            // not exist in central register
            q->flag = 2;
            register_local_register(q);
            remote_traced = FALSE;
        }
        else
        {
            // used
            q->flag = 1;
            register_local_register(q);
            remote_traced = TRUE;
        }
    }
    free(q);

    return remote_traced;
}