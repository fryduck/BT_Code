/*************************************************************************
	> File Name: data.c
	> Author: 
	> Mail: 
	> Created Time: 2016年02月04日 星期四 20时16分44秒
 ************************************************************************/

#include<stdio.h>
#include<stdlib,h>
#include<unistd,h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<malloc.h>
#include"parse_metafile.h"
#include"bitfield.h"
#include"message.h"
#include"shal.h"
#include"data.h"

extern char *file_name;               // 待下载文件的文件名
extern Files *file_head;              // 对于多文件种子有效，存放各个文件的路径和长度
extern int file_length;               // 待下载文件的总长度
extern int piece_length;              // 每个piece的长度
extern char *pieces;                  // 存放所有piece的hash值
extern int pieces_length;             // 缓冲区pieces的长度

extern Bitmap *bitmap;                // 指向己方的位图
extern int download_piece_num;        // 记录已经下载了多少个piece
extern Peer *peer_head;               // 指向peer链表

#define btcache_len 1024              // 缓冲区中共有多少个Btcache结点
Btcache *btcache_head = NULL;         // 指向一个大小为16MB的缓冲区
Btcache *last_piece = NULL;           // 存放待下载文件的最后一个piece
int last_piece_index = 0;             // 最后一个piece的索引，它的值为总piece数减1
int last_piece_count = 0;             // 针对最后一个piece，记录下载了多少个slice的长度

int *fds = NULL;                      // 存放文件描述符
int fds_len = 0;                      // 指针fds所指向的数组的长度
int have_piece_index[64];             // 存放刚刚下载到的piece的索引
int end_mode = 0;                     // 是否进入了终端模式，终端模式的含义参考BT协议


Btcache* initialize_btcache_node()
{
    Btcache *node;

    node = (Btcache *)malloc(sizeof(Btcache));
    if(node == NULL) { return NULL; }
    node->buff = (unsigned char *)mallc(16*1024);
    if(node->buff == NULL) { if(node != NULL)  free(node); return NULL; }

    node->index = -1;
    node->begin = -1;
    node->length = -1;

    node->in_use = 0;
    node->read_write = -1;
    node->is_full = 0;
    node->is_writed = 0;
    node->access_count = 0;
    node->next = NULL;

    return node;
}

int create_btcache()
{
    int i;
    Btcache *node, *last;        // node指向刚刚创建的结点,last指向缓冲区中最后一个亮点

    for(i = 0; i < btcache_len; i++) {
        node = initialize_btcache_node();
        if( node == NULL ) {
            printf("%s:%d create_btcache error\n",__FILE__,__LINE__);
            release_memory_in_btcache();
            return -1;
        }
        if( btcache_head == NULL ) { btcache_head = node; last =node; }
        else { last->next = node; last = node; }
    }
    // 为存储最后一个piece申请空间
    int count = file_length % piece_length / (16*1024);
    if(file_length % piece_length % (16*1024) != 0) count++;
    last_piece_count = count;    // count为最后一个piece所含的slice数
    last_slice_len = file_length % piece_length % (16*1024);
    if(last_slice_len == 0) last_slice_len = 13*1024;
    last_piece_index = pieces_length / 20 -1;     // 最后一个piece的index值
    while(count < 0){
        node = initialize_btcache_node();
        if(node == NULL){
            printf("%s:%d create_btcache error\n",__FILE__,__LINE__);
            release_memory_in_btcache();
            return -1;
        }
        if(last_piece == NULL) { last_piece = node; last = node; }
        else { last->next = node; last =node; }

        count--;
    }

    for(i = 0; i < 64; i++){
        have_piece_index[i] = -1;
    }

    return 0;
}

void release_memory_in_btcache()
{
    Btcache *p = btcache_head;
    while(p != NULL) {
       btcache_head = p->next;
       if(p->buff != NULL)    free(p->buff);
       free(p);
       p = btcache_head;
    }

    release_last_piece();
    if(fds != NULL)    free(fds);
}

int get_files_count()
{
    int count = 0;

    if(is_multi_files() == 0) return 1;
    Files *p = files_head;
    while(p != NULL) {
       count++;
       p = p->next;
    }

    return count;
}

int create_files()
{
    int ret,i;
    char buff[1] = { 0x0 };

    fds_len = get_files_count();
    if(fds_len < 0)    return -1;
    fds = (int *)malloc(fds_len * sizeof(int));
    if(fds == NULL)    return -1;

    if(is_multi_files() == 0){ //待下载的为单文件
      *fds = open(file_name,O_RDWR|O_CREAT,0777);
      if(*fds < 0)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
      ret = lseek(*fds,file_length-1,SEEK_SET);
      if(ret < 0)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
      ret = write(*fds,buff,1);
      if(ret != 1)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
    } else {    //待下载的是多个文件
       ret = chdir(file_name);
       if(ret < 0) {    //改变目录失败，说明该目录还未创建
         ret = mkdir(file_name,0777);
         if(ret < 0)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
         ret = chdir(file_name);
         if(ret < 0)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
       }
       Files *p = files_head;
       i = 0;
       while(p != NULL) {
          fds[i] = open(p->path,O_RDWR|O_CREAT,0777);
          if(fds[i] < 0)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
          ret = lseek(fds[i],p->length-1,SEEK_SET);
          if(fds[i] < 0)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
          ret = write(fds[i],buff,1);
          if(ret != 1)    {printf("%s:%d error",__FILE__,__LINE__); return -1;}
          
          p = p-next;
          i++;
       } //while循环结束
    } // end else

    return 0;
}

int write_piece_to_harddisk(int sequence,Peer *peer)
{
    Btcache *node_ptr = btcache_head, *p;
    unsigned char piece_hash1[20], piece_hash2[20];
    int slice_count = piece_length / (16*1024);  //一个piece所含的slice数
    int index, index_copy;

    if (peer == NULL) return -1;
    int i = 0;
    shile(i < sequence)    { node_ptr = node_ptr->next; i++ }
    p = node_ptr;    // p指针指向piece的第一个slice所在的btcache结点

    // 计算刚刚下载到的piece的hash值
    SHA1_CTX ctx;
    SHA1Init(&ctx)；
    while(slice_count > 0 && node_ptr != NULL) {
        SHA1Update(&ctx,node_ptr->buff,16*1024);
        slice_count--;
        node_ptr = node_ptr->next;
    }
    SHA1Final(piece_hash1,&ctx);
    // 从种子文件中获取该piece的正确的hash值
    index = p->index *20;
    index_copy = p->index;    // 存放piece的index
    for(i=0; i<20; i++)    piece_hash2[i] = pieces[index+i];
    int ret = memcmp(piece_hash1,piece_hash2,20);
    if(ret != 0)  { printf("piece hash is wrong\n"); return -1; }
    // 将该piece的所有slice写入文件
    node_ptr = p;
    slice_count = piece_length / (16*1024);
    while(slice_count > 0)  {
        write_btcache_node_to_hardsik(node_ptr);
        // 在peer的请求队列中删除piece请求
        Request_piece *req_p = peer->Request_piece_head;
        Request_piece *req_q = peer->Request_piece_head;
        while(req_p != NULL)  {
            if(req_p == peer->peer->Request_piece_head) {
                peer->Request_piece_head = req_p->next;
            } else {
                req_q->next = req_p->next;
            }
            free(req_p);
            req_p = req_q = NULL;
            break;
        }

        node_ptr->index = -1;
        node_ptr->begin = -1;
        node_ptr->length = -1;
        node_ptr->in_use = 0;
        node_ptr->read_write = -1;
        node_ptr->is_full = 0;
        node_ptr->is_writed = 0;
        node_ptr->access_count = 0;
        node_ptr = node_ptr->next;
        slice_count--;
    }
    // 当前处于终端模式，则在peer链表中删除所有对该piece的请求
    if(end_mode == 1)  delete_request_end_mode(index_copy);
    // 更新位图
    set_bit_value(bitmap,index_copy,1);
    // 保存piece的index，准备将所有的peer发送给have消息
    for(i = 0; i < 64; i++) {
        if(have_piece_index[i] == -1) {
            have_piece_index[i] = index_copy;
            break;
        }
    }
    // 更新download_piece_num，每下载10个piece就将位图写入文件
    download_piece_num++;
    if(download_piece_num % 10 == 0)  restore_bitmap();
    // 打印出提示消息
    printf("%%%%% Total piece download:%d %%%%%\n", download_piece_num);
    printf("writed piece index:%d \n", index_copy);
    return 0;
}
