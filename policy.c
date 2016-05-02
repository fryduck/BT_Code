/*************************************************************************
	> File Name: policy.c
	> Author: 
	> Mail: 
	> Created Time: Mon 02 May 2016 03:27:02 PM JST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include"parse_metafile.h"
#include"peer.h"
#include"data.h"
#include"message.h"
#include"policy.h"

long long total_down = 0L, total_up = 0L;   // 总的下载量和上传量
float total_down_rate = 0.0F, total_up_rate = 0.0F;     // 总的下载上传速度
int total_peers = 0;    // 已连接的总peer数
Unchoke_peers unchoke_peers;    // 存放非阻塞Peer和优化非阻塞Peer的指针

extern int end_mode;    // 是否已进入终端模式
extern Bitmap *bitmap;  // 指向己方的位图
extern Peer *peer_head;     // 指向Peer链表
extern int pieces_length;   // 所有piece hash值的长度
extern int piece_length;    // 每个piece的长度

extern Btcache *btcache_head;   // 指向存放下载数据的缓冲区
extern int last_piece_index;    // 最后一个piece的index
extern int last_piece_count;    // 最后一个piece所含的slice数
extern int last_slice_len;      // 最后一个piece的最后一个slice的长度
extern int download_piece_num;  // 已下载的piece数

int *rand_num = NULL;

int get_rand_numbers(int length)
{
    int i, index, piece_count, *temp_num;

    if (length == 0)    return -1;
    piece_count = length;

    rand_num = (int *)malloc(piece_count * sizeof(int));
    if (rand_num == NULL)   return -1;

    temp_num = (int *)malloc(piece_count * sizeof(int));
    if (temp_num == NULL)   return -1;
    for (i = 0; i < piece_count; i++)   temp_num[i] = i;

    srand(time(NULL));
    for (i = 0; i < piece_count; i++) {
        index = (int)( (float)(piece_count - 1) * rand() / (RAND_MAX + 1.0) );
        rand_num[i] = temp_num[index];
        temp_num[index] = temp_num[piece_count-1-i];
    }

    if (temp_num != NULL)   free(temp_num);
    return 0;
}

int compute_rate()
{
    Peer *p = peer_head;
    time_t time_now = time(NULL);
    long t = 0;

    while (p != NULL) {
        if (p->last_down_timestamp == 0) {
            p->down_rate = 0.0f;
            p->down_count = 0;
        } else {
            t = time_now - p->last_down_timestamp;
            if (t == 0)     printf("%s:%d time is 0\n", __FILE__, __LINE__);
            else p->down_rate = p->down_count / t;
            p->down_count = 0;
            p->last_down_timestamp = 0;
        }

        if (p->last_up_timestamp == 0) {
            p->up_rate = 0.0f;
            p->up_count = 0;
        } else {
            t = time_now - p->last_up_timestamp;
            if (t == 0)     printf("%s:%d time is 0\n", __FILE__, __LINE__);
            else p->up_date = p->up_count / t;
            p->up_count = 0;
            p->last_up_timestamp = 0;
        }

        p = p->next;
    }
    return 0;
}

int compute_total_rate()
{
    Peer *p = peer_head;

    total_peers = 0;
    total_down = 0;
    total_up = 0;
    total_down_rate = 0.0f;
    total_up_rate = 0.0f;

    while (p != NULL) {
        total_down += p->down_total;
        total_up += p->up_total;
        total_down_rate += p->down_rate;
        total_up_rate += p->up_rate;

        total_peers++;
        p = p->next;
    }
    return 0;
}

int create_req_slice_msg(Peer *node)
{
    int index, being, length = 16*1024;
    int i, count = 0;

    if (node == NULL)   return -1;
    // 如果被peer阻塞或对peer不感兴趣，就没有必要生成request消息
    if (node->peer_choking == 1 || node->am_interested == 0)    return -1;

    // 如果之前向该peer发送过请求，则根据之前的请求构造新请求
    // 遵守一条原则：同一个piece的所有slice应该尽可能地同一个peer处下载
    Request_piece *p = node->Request_piece_head, *q = NULL;
    if (p != NULL) {
        while (p->next != NULL)     { p = p->next; }    //　定位到最后一个节点处
        int last_begin =piece_length - 16*1024;         // 一个piece的最后一个slice的起始下标
        if (p->index == last_piece_index) {     // 如果是最后一个piece
            last_begin = (last_piece_count - 1) * 16 * 1024;
        }

        // 当前piece还有未请求的slice，则构造请求消息
        if (p->begin < last_begin) {
            index = p->index;
            begin = p->begin + 16*1024;
            count = 0;

            while (begin != pieces_length && count < 1) {
                // 如果是最后一个piece的最后一个slice
                　　if (p->index == last_piece_index) {
                    if (begin == (last_piece_count - 1) * 16 * 1024)
                        length = last_slice_len;
                }
                // 创建request消息
                create_request_msg(index,begin,length,node);
                // 将当前的请求记录到请求队列
                q = (Request_piece *)malloc(sizeof(Request_piece));
                if (q == NULL) {
                    printf("%s:%d error\n", __FILE__, __LINE__);
                    return -1;
                }
                q->index = index;
                q->begin = begin;
                q->lengh = length;
                q->next = NULL;
                p->next = q;
                p = q;
                begin += 16*1024;
                count++;
            } // end while
            return 0;   // 构造完毕，就返回
        }   // end if(p->begin < last_begin)
    }   // end if(p!=NULL)

    // 开始对一个未请求过的piece发出请求
    if (get_rand_numbers(pieces_length/20) == -1) {     // 生成随机数
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }
    // 随机选择一个piece的下标，该下标所代表的piece应该没有向任何peer请求过
    for (i = 0; i < pieces_length/20; i++) {
        index = rand_num[i];
        // 判断对于以index为下标的piece,peer是否拥有
        if (get_bit_value(&(node->bitmap),index) != 1)  continue;
        if (get_bit_value(bitmap,index) == 1)   continue;
        // 判断对于以index为下标的piece是否已经请求过了
        Peer *peer_ptr = peer_head;
        Request_piece *reqt_ptr;
        int find = 0;
        while (peer_ptr != NULL) {
            reqt_ptr = peer_ptr->Request_piece_head;
            while (reqt_ptr != NULL) {
                if (reqt_ptr->index == index)   { find = 1; break; }
                reqt_ptr = reqt_ptr->next;
            }
            if (find == 1)  break;
            peer_ptr = peer_ptr->next;
        }
        if (find == 1)  continue;
        break;  // 程序若执行到此处，说明已经找到一个符合要求的index
    }
    /* 如果还未找到一个合适的index，说明所有的piece要么已经被下载要么正在被请求下载，
     * 而此时还有多余的对本客户端解除阻塞的peer，说明已经进入终端模式，即将下载完成*/
    if (i == piece_length/20) {
        if (end_mode == 0)  end_mode = 1;
        for (i = 0; i < pieces_length/20; i++) {
            if (get_bit_value(bitmap,i) == 0)   { index = i; break; }
        }
        if (i == pieces_length/20) {
            printf("Can not find an index to IP:%s\n", node->ip);
            return -1;
        }
    }

    // 构造piece请求消息
    begin = 0;
    count = 0;
    p = node->Request_piece_head;
    if (p != NULL) {
        while (p->next != NULL)     p = p->next;
        while (count < 4) {
        // 如果是构造最后一个piece的请求消息
            if (index == last_piece_count) {
                break;
            }
            if (begin == (last_piece_count - 1)*16*1024)
                length = last_slice_len;
        }
        // 创建request消息
        create_request_msg(index,begin,length,node);
        // 将请求记录到请求队列
        q = (Request_piece *)malloc(sizeof(Request_piece));
        if (q == NULL)  { printf("%s:%d error\n", __FILE__, __LINE__); return -1; }
        q->index = index;
        q->begin = begin;
        q->length = length;
        q->next = NULL;
        if (node->Request_piece_head == NULL) {
            node->Request_piece_head = q;
            p = q;
        } else {
            p->next = q;
            p = q;
        }
        begin += 16*1024;
        count++;
    }

    if (rand_num != NULL)   { free(rand_num); rand_num = NULL; }
    return 0;
}
