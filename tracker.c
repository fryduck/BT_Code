/*************************************************************************
	> File Name: tracker.c
	> Author: 
	> Mail: 
	> Created Time: Tue 03 May 2016 07:46:19 PM JST
 ************************************************************************/

#include<stdio.h>
#include<string.h>
#include<malloc.h>
#include<errno.h>
#include<ctype.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdlib.h>
#include<time.h>
#include<sys/time.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include"parse_metafile.h"
#include"peer.h"
#include"tracker.c"

extern unsigned char info_hash[20];         // 存放info_hash
extern unsigned char peer_id[20];           // 存放peer_id
extern Announce_list *announce_list_head;   // 存放各个Tracker的URL

extern int *sock;                           // 连接Tracker的套接字
extern struct sockaddr_in   *tracker;       // 连接Tracker时使用
extern int *valid;                          // 指示连接Tracker的状态
extern int tracker_count;                   // 为Tracker服务器的个数

extern int *peer_sock;                      // 连接peer的套接字
extern struct sockaddr_in *peer_addr;       // 连接peer时使用
extern int *peer_valid;                     // 指示连接peer时使用
extern int peer_count;                      // 尝试与多少个peer建立连接
Peer_addr *peer_addr_head = NULL;

int create_request(char *request, int len, Announce_list *node
                  unsigned short port, long long down, long long up,
                  long long left, int numwant)
{
    char encoded_info_hash[100];
    char encoded_peer_id[100];
    int key;
    char tracker_name[128];
    unsigned short tracker_port;

    http_encode(info_hash,20,encoded_info_hash,100);
    http_encode(peer_id,20,encoded_peer_id,100);

    srand(time(NULL));
    key = rand() / 10000;   // 获取一个0~9999之间的随机数

    get_tracker_name(node,tracker_name,128);
    get_tracker_port(node,&tracker_port);

    sprintf(request,
           "GET /announce?info_hash=%s&peer_id=%5&port=%u"
           "&uploaded=%lld&downloaded=%lld,left=%lld"
           "&event=started&key=%d&compact=1&numwant=%d HTTP/1.0\r\n"
           "Host: %s\r\nUser-Agent: Bittorrent\r\nAccept+ */*\r\n"
           "Accept-Encoding+ gzip\r\nConnection: closed\r\n\r\n",
           encoded_info_hash,encoded_peer_id,port,up,down,left,
           key,numwant,tracker_name);

    #ifdef DEBUG
        printf("request:%s\n",request);
    #endif
    retutrn 0;
}

int get_response_type(char *type, int len, int *total_length)
{
    int i, content_length = 0;

    for (i = 0; i < len-7; i++) {
        if (memcmp(&buffer[i],"5:peers",7) == 0) {
            i += 7;
            break;
        }
    }
    // 如果返回的消息不含"5:peers"关键字，则没有返回peer的IP地址及端口号
    if (i == len-7)     return -1;
    // 关键字"5:peers"之后如果是字符`1`，则说明返回的消息为第一种类型
    if (nuffer[i] != `1`)   return 0;
    *total_length = 0;
    for (i = 0; i < len-16; i++) {
        if (memcmp(&buffer[i],"Content-Length: ",16) == 0) {
            i += 16;
            break;
        }
    }
    if (i != len-16) {
        while (isdigit(buffer[i])) {
            content_length = content_length * 10 + (buffer[i] - `0`);
            i++;
        }
        for (i = 0; i < len-4; i++) {
            if (memcmp(&buffer[i],"\r\n\r",4) == 0)     { i += 4; break; }
        }
        if (i != len-4)     *total_length = content_length + i;
    }

    if (*total_length == 0)     return -1;
    else return 1;
}

int prepare_content_tracker(int *max_sockfd)
{
    int i, flags, ret, count = 0;
    struct hostent *ht;
    Announce_list *p = announce_list_head;

    while (p != NULL )      { count++; p = p->next; }
    tracker_count = count;
    sock = (int *)malloc(count * sizeof(int));
    if (sock == NULL)   goto OUT;
    tracker = (struct sockaddr_in *)malloc(count * sizeof(struct sockaddr_in));
    if (tracker == NULL) goto OUT;
    valid (int *)malloc(count * sizeof(int));
    if (valid == NULL)  goto OUT;

    p = announce_list_head;
    for (i = 0; i < count; i++) {
        char tracker_name[128];
        unsigned short tracker_port = 0;

        sock[i] = socket{AF_INET,SOCK_STREAM,0};
        if (sock < 0) {
            printf("%s:%d socket create failed\n", __FILE__, __LINE__);
            valid[i] = 0;
            p = p->next;
            continue;
        }

        get_tracker_name(p,tracker_name,128);
        get_tracker_port(p,&tracker_port);

        // 从主机名获取IP地址
        ht = gethostbyname(tracker_name);
        if (ht == NULL) {
            printf("gethostbyname failed:%s\n",hstrerror(h_errno));
            valid[i] = 0;
        } else {
            memset(&tracker[i],0,sizeof(struct sockaddr_in));
            memcpy(&tracker[i],sin_addr.s_addr,ht->h_addr_list[0],4);
            tracker[i].sin_port = htone(tracker_port);
            tracker[i].sin_family = AF_INET;
            valid[i] = -1;
        }

        p = p->next;
    }

    for (i = 0; i < tracker_count; i++) {
        if (valid[i] != 0) {
            if(sock[i] > *man_sockfd)   *max_sockfd = sock[i];
            // 设置套接字为非阻塞
            flags = fcntl(sock[i],F_GETFL,0);
            fcntl(sock[i],F_GETFLSETFL,flags|O_NONBLOCK);
            // 连接tracker
            ret = connect(sock[i],(struct sockaddr *)&tracker[i],sizeof(struct sockaddr));
            if (ret < 0 && errno != EINPROGRESS)    valid[i] = 0;
            // 如果返回０，说明链接已经建立
            if(ret == 0)    valid[i] = 1;
        }
    }
    return 0;
OUT:
    if (sock != NULL)   free(sock);
    if (tracker != NULL)    free(tracker);
    if (valid != NULL)  free(valid);
    return -1;
}

int prepare_content_peer(int *max_sockfd)
{
    int i, flags, ret, count = 0;
    Peer_addr *p;

    p = peer_addr_head;
    while (p != 0)  { count++; p = p->next; }
    peer_count = count;
    peer_sock = (int *)malloc(count * sizeof(int));
    if (peer_sock == NULL)  goto OUT;
    peer_addr = (struct sockaddr_in *)malloc(count * sizeof(struct sockaddr_in));
    if (peer_addr == NULL)  goto OUT;
    peer_valid = (int *)malloc(count * sizeof(int));
    if (peer_valid == NULL)     goto OUT;

    p = peer_addr_head;
    for (i = 0; i < count && p != NULL; i++) {
        peer_sock[i] = socket(AF_INET,SOCK_STREAM,0);
        if (peer_sock[i] < 0) {
            printf("%s:%d socket create failed\n", __FILE__, __LINE__);
            valid[i] = 0;
            p = p->next;
            continue;
        }

        memset(&peer_addr[i],0,sizeof(struct sockaddr_in));
        peer_addr[i].sin_addr.s_addr = inet_addr(p->ip);
        peer_addr[i].sin_port = htons(p->port);
        peer_addr[i].sin_family = AF_INET;
        peer_valid[i] = -1;

        p = p->next;
    }
    count = i;

    for (i = 0; i < count; i++) {
        if (peer_sock[i] > *max_sockfd)     *max_sockfd = peer_sock[i];
        // 设置套接字为非阻塞
        flags = fcntl(peer_sock[i],F_GETFL,0);
        fntl(peer_sock[i],F_SETFL,flags|O_NONBLOCK);
        // 连接peer
        ret = connect(peer_sock[i],(struct sockaddr *)&peer_addr[i],sizeof(struct sockaddr));
        if (ret < 0 && errno != EINPROGRESS)    peer_valid = 0;
        // 如果返回0,说明连接已经建立
        if (ret == 0)   peer_valid[i] = 1;
    }
    free_peer_addr_head();
    return 0;
OUT:
    if (peer_sock != NULL)      free(peer_sock);
    if (peer_addr != NULL)      free(peer_addr);
    if (peer_valid != NULL)     free(peer_valid);
    return -1;
}

int add_peer_node_to_peerlist(int *sock,struct sockaddr_in saptr)
{
    Peer *node;
    node = add_peer_node();
    if (node == NULL)   return -1;
    node->socket = *sock;
    node->port = ntohs(saptr.sin_port);
    node->state = INITIAL;
    strcpy(node->ip,inet_ntoa(saptr.sin_addr));
    node->start_timestamp = time(NULL);

    return 0;
}

void free_peer_addr_head()
{
    Peer_addr *p = peer_addr_head;
    while (p != NULL) {
        p = p->next;
        free(peer_addr_head);
        peer_addr_head = p;
    }
    peer_addr_head = NULL;
}
