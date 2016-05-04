/*************************************************************************
	> File Name: torrent.c
	> Author: 
	> Mail: 
	> Created Time: Wed 04 May 2016 08:37:05 PM JST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<netdb.h>
#include<errno.h>
#include"torrent.h"
#include"message.h"
#include"tracker.h"
#include"peer.h"
#include"policy.h"
#include"data.h"
#include"bitfield.h"
#include"parse_metafile.h"

// 接收缓冲区中的数据达到threshold时，需要立即进行处理，否则缓冲区可能会溢出
// 18*1024bit即18KB是接收缓冲区的大小，1500Byte是以太网等局域网中一个数据包的最大长度
#define threshold (18*1024-1500)

extern Announce_list    *announce_list_head;
extern char             *file_name;
extern long long        *file_length;
extern int              file_length;
extern char             piece_length;
extern char             *pieces;
extern int              pieces_length;
extern Peer             *peer_head;

extern long long        total_down, total_up;
extern float            total_down_date, total_up_rate;
extern int              total_peers;
extern int              download_piece_num;
extern Peer_addr        *peer_addr_head;

int *sock = NULL;       // 连接Tracker的套接字
struct sockaddr_in  *tracker = NULL;        // 连接Tracker时使用
int *valid = NULL;      // 指示连接Tracker的状态
int tracker_count = 0;      // 为Tracker服务器的个数

int *peer_sock = NULL;      // 链接peer的套接字
struct sockaddr_in *peer_addr = NULL;       // 连接peer时使用
int *peer_valid = NULL;         // 指示连接peer时的状态
int peer_count = 0;         // 尝试与多少个peer建立连接

int response_len = 0;       // 存放Tracker回应的总长度
int reponse_index = 0;      // 存放Tracker回应当前长度
char *tracker_response = NULL       // 存放Tracker回应

int download_upload_with_peers()
{
    Peer *p;
    int ret, max_sockfd, i;

    int connect_tracker, connecting_tracker;
    int connect_peer, connecting_peer;
    time_t last_time[3], now_time;

    time_t start_connect_tracker;       // 开始连接Tracker的时间
    time_t start_connect_peer;          // 开始连接peer的时间
    fd_set rset, wset;      // select受监视的描述符集合
    struct timeval  tmval;      // select函数的随机时间

    now_time = time(NULL);      
    last_time[0] = now_time;        // 上一次选择非阻塞peer的时间
    last_time[1] = now_time;        // 上一次选择优化非阻塞peer的时间
    last_time[2] = now_time;        // 上一次连接Tracker服务器的时间
    connect_tracker = 1;            // 是否需要连接Tracker
    connecting_tracker = 0;         // 是否正在连接Tracker
    connect_peer = 0;               // 是否需要连接peer
    connecting_peer = 0;            // 是否正在连接peer

    for (; ;) {
        max_sockfd = 0;
        now_time = time(NULL);

        // 每隔10秒重新选择非阻塞peer
        if (now_time - last_time[0] >= 10) {
            if (download_piece_num > 0 && peer_head != NULL) {
                compute_rate();     // 计算各个peer的下载,上传速度
                select_unchoke_peer();   // 选择非阻塞的peer
                last_time[0] = now_time;
            }
        }
        // 每隔30秒重新选择优化非阻塞peer
        if (now_time - last_time[1] >= 30) {
            if (download_piece_num > 0 && peer_head != NULL) {
                select_optunchoke_peer();
                last_time[1] = now_time;
            }
        }
        // 每隔5分钟重新连接一次Tracker，如果当前peer数为０也连接Tracker
        if (now_time - last_time[2] >= 300 || connect_tracker == 1 && 
           connecting_tracker != 1 && connect_peer != 1 && connecting_peer != 1) {
               // 由Tracker的URL获取Tracker的IP地址和端口号
               ret = prepare_connect_tracker(&max_sockfd);
               if (ret < 0)     { printf("prepare_connect_tracker\n"); return -1; }
               connect_tracker = 0;
               connecting_tracker = 1;
               start_connect_tracker = now_time;
        }
        // 如果要连接新的peer，做准备工作
        if (connect_peer == 1) {
            // 创建套接字，向peer发出连接请求
            ret = prepare_connect_tracker(&max_sockfd);
            if (ret < 0)    { printf("prepare_connect_tracker"); return -1; }

            connect_peer = 0;
            connecting_peer = 1;
            start_connect_peer = now_time;
        }

        FD_ZERO(&rset);
        FD_ZERO(&wset);

        // 将连接Tracker的socket加入到待监视的集合中
        if (connecting_tracker == 1) {
            int flag = 1;
            // 如果连接Tracker超过10秒，则终止连接Tracker
            if (now_time - start_connect_tracker > 10) {
                for (i = 0; i < tracker_count; i++)
                    if (valid[i] != 0)  close(sock[i]);
            } else {
                for (i = 0; i < tracker_count; i++) {
                    if (valid[i] != 0 && sock[i] > max_sockfd) {
                        max_sockfd = sock[i];   // valid[i]值为-1,1,2时要监视
                    }
                    if (valid[i] == -1) {
                        FD_SET(sock[i],&rset);
                        FD_SET(sock[i],&wset);
                        if (flag == 1)  flag = 0;
                    } else if (valid[i] == 1) {
                        FD_SET(sock[i],&wset);
                        if (flag == 1)  flag = 0;
                    } else if (valid[i] == 2) {
                        FD_SET(sock[i],&rset);
                        if (flag == 1)  flag = 0;
                    }
                }
            }
            // 说明连接Tracker结束，开始与peer建立连接
            if (flag == 1) {
                connecting_tracker = 0;
                last_time[2] = now_time;
                clear_connect_tracker();
                clear_tracker_response();
                if (peer_addr_head != NULL) {
                    connect_tracker = 0;
                    connect_peer = 1;
                } else {
                    connect_tracker = 1;
                }
                continue;
            }
        }
        // 将正在连接peer的socket加入到待监视的集合中
        if (connecting_peer == 1) {
            int flag = 1;
            // 如果连接peer超过10秒，则终止连接peer
            if (now_time - start_connect_tracker > 10) {
                for (i = 0; i < peer_count; i++) {
                    if (peer_valid[i] != 1)     close(peer_sock[i]);    // 不为1说明连接失败
                }
            } else {
                for (i = 0; i < peer_count; i++) {
                    if (peer_valid[i] == -1) {
                        if (peer_sock[i] > max_sockfd)
                            max_sockfd = peer_sock[i];
                        FD_SET(peer_sock[i],&rset);
                        FD_SET(peer_sock[i],&wset);
                        if (flag == 1)  flag = 0;
                    }
                }
            }
            if (flag == 1) {
                connecting_peer = 0;
                clear_connect_peer();
                if (peer_head == NULL)  connect_tracker = 1;
                continue;
            }
        }

        // 将peer的socket成员加入到待监视的集合中
        connect_tracker = 1;
        p = peer_head;
        while (p != NULL) {
            if (p->state != CLOSING && p->socket > 0) {
                FD_SET(p->socket,&rset);
                FD_SET(p->socket,&wset);
                if (p->socket > max_sockfd)     max_sockfd = p->socket;
                connect_tracker = 0;
            }
            p = p->next;
        }
        if (peer_head == NULL && (connecting_tracker == 1 || connecting_peer == 1))
            connect_tracker = 0;
        if (connect_tracker == 1)   continue;

        // 调用select库函数监视各个套接字是否可读写
        tmval.tv_sec = 2;
        tmval.tv_usec = 0;
        ret = select(max_sockfd+1,&rset,&wset,NULL,&tmval);
        if (ret < 0) {  // select出错
            printf("%s:%d error\n", __FILE__, __LINE__);
            perror("select error");
            break;
        }
        if (ret == 0)   continue;   // select超时

        // 添加have消息，have消息要发送给没一个peer，放在此处是为了方便处理
        prepare_send_have_msg();
        // 对于每个peer，接收或发送消息，接收到一条完整的消息就进行处理
        p = peer_head;
        while (p != NULL) {
            if (p->state != CLOSING && FD_ISSET(p->socket,&rset)) {
                ret = recv(p->socket,p->in_buff+p->buff_len,MSG_SIZE-p->buff_len,0);
                if (ret <= 0) {     // recv返回0说明对方关闭连接，返回负数说明出错
                    p->state = CLOSING;
                    // 通过设置套接字选项来丢弃发送缓冲区中的数据
                    discard_send_buffer(p);
                    clear_btcache_before_peer_close(p);
                    close(p->socket);
                } else {
                    int_completed, ok_len;
                    p->buff_len += ret;
                    completed = is_complete_message(p->in_buff,p->buff_len,&ok_len);
                    if (completed == 1)     parse_response(p);
                    else if (p->buff_len >= threshold)
                        parse_response_uncomplete_msg(p,ok_len);
                    else
                        p->start_timestamp = time(NULL);
                }
            }
            if (p->state != CLOSING && FD_ISSET(p->socket,&wset)) {
                if (p->msg_copy_len == 0) {
                    //　创建待发送的消息，并把生成的消息拷贝到发送缓冲区并发送
                    create_response_message(p);
                    if (p->msg_len > 0) {
                        memcpy(p->out_msg_copy,p->out_msg,p->msg_len);
                        p->msg_copy_len = p->msg_len;
                        p->msg_len = 0;     // 清空p->out_msg所存的消息
                    }
                }
                if (p->msg_copy_len > 1024) {
                    send(p->socket,p->out_msg_copy+p->msg_copy_index,1024,0);
                    p->msg_copy_len = p->msg_copy_len - 1024;
                    p->msg_copy_index = p->msg_copy_index + 1024;
                    p->recet_timestamp = time(NULL);
                }
                else if (p->msg_copy_len <= 1024 && p->msg_copy_len > 0) {
                    send(p->socket,p->out_msg_copy+p->msg_copy_index,p->msg_copy_len,0);
                    p->msg_copy_len = 0;
                    p->msg_copy_index = 0;
                    p->recet_timestamp = time(NULL);
                }
            }
            p = p->next;
        }

        if (connecting_tracker == 1) {
            for (i = 0; i < tracker_count; i++) {
                if (valid[i] == -1) {
                    // 如果某个套接字可写且未发生错误，说明连接建立成功
                    if (FD_ISSET(sock[i],&wset)) {
                        int error, len;
                        error = 0;
                        len = sizeof(error);
                        ret = getsockopt(sock[i],SOL_SOCKET,SO_ERROR,&error,&len);
                        if (ret < 0) {
                            valid[i] = 0;
                            close(sock[i]);
                        }
                        if (error)  { valid[i] = 0; close(sock[i]); }
                        else    { valid[i] = 1; }
                    }
                }
                if (valid[i] == 1 && FD_ISSET(sock[i],&wset)) {
                    char request[1024];
                    unsigned short listen_port = 33550;     // 本程序并未实现监听某端口
                    unsigned long down = total_down;
                    unsigned long up = total_up;
                    unsigned long left;
                    left = (pieces_length/20-download_piece_num)*piece_length;

                    int num = i;
                    Announce_list *anouce = announce_list_head;
                    while (num > 0) {
                        anouce = anouce->next;
                        num--;
                    }
                    create_request(request,1024,anouce,listen_port,down,up,left,200);
                    write(sock[i],request,strlen(request));
                    valid[i] = 2;
                }
                if (valid[i] == 2 && FD_ISSET(sock[i],&rset)) {
                    char buffer[2048];
                    char redirection[128];
                    ret = read(sock[i],buffer,sizeof(buffer));
                    if (ret > 0) {
                        if (response_len != 0) {
                            memcpy(tracker_response+reponse_index,buffer,ret);
                            reponse_index += ret;
                            if (reponse_index == reponse_len) {
                                parse_tracker_reponse2(tracker_response,reponse_len);
                                clear_tracker_response();
                                valid[i] = 0;
                                close(sock[i]);
                                last_time[2] = time(NULL);
                            }
                        } else if (get_reponse_type(buffer,ret,&reponse_len) == 1) {
                            tracker_response = (char *)malloc(reponse_len);
                            if (tracker_response == NULL)   printf("malloc error\n");
                            memcpy(tracker_response,buffer,ret);
                            reponse_index = ret;
                        } else {
                            ret = parse_tracker_reponse1(buffer,ret,redirection,128);
                            if (ret == 1)   add_an_announce(redirection);
                            valid[i] = 0;
                            close(sock[i]);
                            last_time[2] = time(NULL);
                        }   // if (reponse_len != 0)
                    }   // end if (ret > 0)
                }   // end if (valid[i] == 1 && FD_ISSET(sock[i],&wset))
            }   // end for (i = 0; i < tracker_count; i++)
        }   // end if (valid[i] == -1)

        if (connecting_peer == 1) {
            for (i = 0; i < peer_count; i++) {
                if (peer_valid[i] == -1 && FD_ISSET(peer_sock[i],&wset)) {
                    int error, len;
                    error = 0;
                    len = sizeof(error);
                    ret = getsockopt(peer_sock[i],SOL_SOCKET,SO_ERROR,&error,&len);
                    if (ret < 0) {
                        peer_valid = 0;
                    }
                    if (error == 0) {
                        peer_valid[i] = 1;
                        add_peer_node_to_peerlist(&peer_sock[i],Peer_addr[i]);
                    }
                }   // if结束语句
            }   // for结束语句
        }   // if结束语句

        // 对处于CLOSING状态的peer，将其从peer队列中解除
        // 此处应当非常小心，处理不当容易使程序崩溃
        p = peer_head;
        while (p != NULL) {
            if (p->state == CLOSING) {
                del_peer_node(p);
                p = peer_head;
            } else {
                p = p->next;
            }
        }
        // 判断是否已经下载完毕
        if (download_piece_num == pieces_length/20) {
            printf("+++++ All Files Downloaded Successfully +++++\n");
            break;
        }
    } // end for(; ;)

    return 0;
}

void clear_connect_tracker()
{
    if (sock != NULL)   { free(sock); sock = NULL; }
    if (tracker != NULL)    { free(Tracker); tracker = NULL; }
    if (valid != NULL)  { free(valid); valid = NULL; }
    tracker_count = 0;
}

void clear_connect_peer()
{
    if (peer->sock != NULL)     { free(peer_sock); peer_sock = NULL; }
    if (peer->addr != NULL)     { free(Peer_addr); Peer_addr = NULL; }
    if (peer_valid != NULL)     { free(peer_valid); peer_valid = NULL; }
    peer_count = 0;
}

void clear_tracker_response();
{
    if (tracker_response != NULL) {
        free(tracker_response);
        tracker_response = NULL;
    }
    response_len = 0;
    reponse_index = 0;
}
