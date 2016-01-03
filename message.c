/*************************************************************************
	> File Name: message.c
	> Author: 
	> Mail: 
	> Created Time: 2015年11月29日 星期日 12时33分47秒
 ************************************************************************/

#include<stdio.h>
#include<string.h>
#include<malloc.h>
#include<unist.d>
#include<stdlib.h>
#include<time.h>
#include<sys/socket.h>
#include"parse_metafile.h"
#include"bitfield.h"
#include"peer.h"
#include"policy.h"
#include"data.h"
#include"message.h"


#define RANDSHAKE            -2    // 握手消息
#define KEEP_ALIVE           -1    // keep_alive消息
#define CHOKE                 0    // choke 消息
#define UNCHOKE               1    // unchoke 消息
#define INTERESTED            2    // interested 消息
#define UNINTERSETED          3    // uninterested 消息
#define HAVE                  4    // have 消息
#define BITFIELD              5    // bitfiled 消息
#define REQUEST               6    // request 消息
#define PIECE                 7    // piece 消息
#define CANCEL                8    // cancel 消息
#define PORT                  9    // port 消息

// 如果45秒未给某peer发送消息，则发送keep_alive消息
#define KEEP_ALIVE_TIME       45    

extern Bitmap *bitmap;             // 在bitmap.c中定义，指向己方的位图
extern char info_hash[20];         // 在parse_metafile.c中定义，存放info_hash
extern char peer_id[20];           // 在parse_metafile.c中定义，皴法功能peer_id
extern int have_piece_index[64];   // 在data.c中定义，存放下载到的piece的index
extern Peer *peer_head;            // 在peer.c中定义，指向peer链表


int int_to_char(int i,unsigned char c[4])
{
    c[3] = i%256;
    c[2] = (i-c[3])/256%256;
    c[1] = (i-c[3]-c[2]*256)/256/256%256;
    c[0] = (i-c[3]-c[2]*256-c[1]*256*256)/256/256/256%256;

    return 0;
}

int char_to_int(unsigned char c[4])
{
    int i;

    i = c[0]*256*256*256+c[1]*256*256+c[2]*256+c[3];

    return i;
}

int create_handshake_msg(char *info_hash,char *peer_id,Peer *peer)
{
    int i;
    unsigned char keyword[2] = "BitTorrent protocol", c = 0x00;
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < 68) return -1;  // 握手消息的长度固定未68字节

    buffer[0] = 19;
    for(i = 0; i < 19; i++)        buffer[i+1] = keyword[i];
    for(i = 0; i < 8; i++)         buffer[i+20] = c;
    for(i = 0; i < 20; i++)        buffer[i+28] = info_hash[i];
    for(i = 0; i < 20; i++)        buffer[i+48] = peer_id[i];

    peer->msg_len += 68;
    return 0;
}

int create_keep_alive_msg(Peer *peer)
{
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < 4)    return -1;    // keep_alive消息的长度固定为4
    memset(buffer,0,4);
    peer->msg_len += 4;
    return 0;
}

int create_chock_interested_msg(int type,Peer *peer)
{
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    // chock,unchoke,interested,uninterested消息的长度固定为5
    if(len < 5)    return -1;
    memset(buffer,0,5);
    buffer[3] = 1;
    buffer[4] = type;

    peer->msg_len += 5;
    return 0;
}

int create_have_msg(int index,Peer *peer)
{
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;
    unsigned char c[4];

    if(len < 9)    return -1;    // have消息的长度固定为为9
    memset(buffer,0,9);
    buffer[3] = 5;
    buffer[4] = 4;
    int_to_char(index,c);        // index为piece的下标
    buffer[5] = c[0];
    buffer[6] = c[1];
    buffer[7] = c[2];
    buffer[8] = c[3];

    peer->msg_len += 9;
    return 0;
}

int create_bitfiled_msg(char *bitfield,int bitfield_len,Peer *peer)
{
    int i;
    unsigned char c[4];
    unsigned char *buffer = peer->out_msg + peer->len;
    int len = MSG_SIZE - peer->msg_len;
    if( len < bitfiled_len + 5 ) {//bitfield消息的长度为bitfield_len+5
        printf("%s:%d buffer too small\n",__FILE__,__LINE__);
        return -1;
    }
    int_to_char(bitfield_len+1,c);    //位图消息的负载长度为位图长度加1
    for(i=0; i <bitfield_len; i++)   buffer[i+5] = bitfield[i];
    peer->msg_len + bitfield_len + 5;
    return 0;
}

int create_request_msg(int index,int begin,int length,Peer *peer)
{
    int i;
    unsigned char c[4];
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    if(len < 17)    return -1;    // request消息的长度固定为17
    memset(buffer,0,17);
    buffer[3] = 13;
    buffer[4] = 6;
    int_to_char(index,c);
    for(i = 0; i < 4; i++)    buffer[i+5] = c[i];
    int_to_char(begin,c);
    for(i = 0; i < 4; i++)    buffer[i+9] = c[i];
    int_to_char(length,c)
    for(i = 0; i < 4; i++)    buffer[i+13] = c[i];
    peer->msg_len += 17;
    return 0;
}

int create_piece_msg(int index,int begin,char *block,int b_len,Peer *peer)
{
    int i;
    unsigned char c[4];
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;
    if( len < b_len+13 )  {  // piece消息的长度为b_len+13
        printf("%s:%d buffer too small\n",__FILE__,__LINE__);
        return -1;
    }
    int_to_char(b_len+9,c);
    for(i = 0; i < 4; i++)    buffer[i] = c[i];
    buffer[4] = 7;
    int_to_char(index,c);
    for(i = 0; i < 4; i++)    buffer[i+5] = c[i];
    int_to_char(begin,c);
    for(i = 0; i < 4; i++)    buffer[i+9] = c[i];
    for(i = 0; i < b_len; i++)    buffer[i+13] = block[i];
    peer->msg_len += b_len+13;
    return 0;
}

int process_handshake_msg(Peer *peer,unsigned char *buffer,int len)
{
    if(peer=NULL || buffer==NULL)    return -1;
    if(memcmp(info_hash,buff+,20) != 0)    { // 若info_hash不一致则关闭连接
        peer->state = CLOSING;
        // 丢弃发送缓冲区中的数据
        discard_send_buffer(peer);
        clear_btcache_before_peer_close(peer);
        close(peer->socket);
        return -1;
    }
    // 保存该peer的peer_id
    memcpy(peer->id,buffer+48,20);
    (peer->id)[20] = '\0';
    // 若当前处于Initial状态，则发送握手消息给peer
    if(peer->state == INITIAL){
        create_handshake_msg(info_hash,peer_id,peer);
        peer->state = HANDSHAKED;
    }
    // 若握手消息已发送，则状态转换为已握手状态
    if(peer->state == HALFSHAKED)    peer->state = HANDSHAKED;
    // 记录最近收到该peer消息的时间
    // 若一定时间内（如两分钟）未收到来自该peer的任何消息，则关闭连接
    peer->state_timestamp = time(NULL);
    return 0;
}

int process_keep_alive_msg(Peer *peer,unsigned char *buff,int len)
{
    if (peer==NULL || buff==NULL) return -1;
    // 记录最近收到该peer消息的时间
    // 若一定时间内（如2min）未收到该peer的任何消息，则关闭连接
    peer->start_timestamp = time(NULL);
    return 0;
}

int process_choke_msg(Peer *peer,unsigned char *buff,int len)
{
    if (peer==NULL || buff==NULL) return -1;
    // 若原先处于unchoke状态，收到该消息后更新peer中某些变量的值
    if (peer->state != CLOSING && peer->peer_choking == 0) {
        peer->peer_choking = 1;
        peer->last_down_timestamp = 0;    // 将最近接收到来自该peer数据的时间清零
        peer->down_count = 0;             // 将最近从该peer处下载的字节数清零
        peer->down_rate = 0;              // 将最近从该peer处下载数据的速度清零
    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int process_unchoke_msg(Peer *peer,unsigned char *buff,int len)
{
    if(peer == NULL || buff==NULL) return -1;
    // 若原来处于choke状态且与该peer的连接未被关闭
    if (peer->state != CLOSING && peer->peer_choking == 1) {
        peer->peer_choking = 0;
       // 若对该peer感兴趣，则构造request消息请求peer发送数据
       if (peer->am_interested == 1) create_req_slice_msg(peer);
       else {
           peer->am_interested = is_interested(&(peer->bitmap), bitmap);
           if (peer->am_interested == 1) create_req_slice_msg(peer);
           else printf("Received unchoke but Not interested to IP:%s \n",peer->ip);
       }
       // 跟新一些成员的值
       peer->last_down_timestamp = 0;
       peer->down_count = 0;
       peer->down_rate = 0;
    }

    peer->state_timestamp = time(NULL);
    return 0;
}

int process_interested_msg(Peer *peer,unsigned char *buff,int len)
{
    if(peer == NULL || buff==NULL) return -1;
    // 若原来处于choke状态且与该peer的连接未被关闭
    if (peer->state != CLOSING && peer->state == DATA) {
        peer->peer_interested = is_interested(bitmap, &(peer->bitmap));
        if (peer->am_interested == 0) return -1;
        if (peer->am_choking == 0) create_chock_interested_msg(1, peer);
    }

    peer->state_timestamp = time(NULL);
    return 0;
}

int process_uninterested_msg(Peer *peer,unsigned char *buff,int len)
{
    if(peer == NULL || buff==NULL) return -1;
    // 若原来处于choke状态且与该peer的连接未被关闭
    if (peer->state != CLOSING && peer->state == DATA) {
        peer->peer_interested = 0;
        cancel_requested_list(peer);
    }

    peer->state_timestamp = time(NULL);
    return 0;
}

int process_have_msg(Peer *peer,unsigned char *buff,int len)
{
    int rand_num;
    unsigned char c[4];

    if(peer == NULL || buff==NULL) return -1;
    srand(time(NULL));
    rand_num = rand() % 3;    // 生成一个0～2的随机数
    // 若原来处于choke状态且与该peer的连接未被关闭
    if (peer->state != CLOSING && peer->state == DATA) {
        c[0] = buff[5]; c[1] = buff[6];
        c[2] = buff[7]; c[3] = buff[8];
        // 更新该peer的位图
        if (peer->bitmap.bitfield != NULL)
        set_bit_value(&(peer->bitmap), char_to_int(c),1);
        if (peer->am_interested == 0) {
            peer->am_interested = is_interested(&(peer->bitmap), bitmap);
            // 由原来的对peer不感兴趣变为敢兴趣时，发送interested消息
            if (peer->am_interested == 1) create_chock_interested_msg(2, peer);
        } else {    // 收到3个have则发1个interested消息 
            if (rand_num == 0) create_chock_interested_msg(2, peer);
        }
    }

    peer->state_timestamp = time(NULL);
    return 0;
}

int process_bitfield_msg(Peer *peer,unsigned char *buff,int len)
{
    unsigned char c[4];

    if(peer == NULL || buff==NULL) return -1;
    if (peer->state != HANDSHAKED && peer->state == SENDBITFIELD) {
        c[0] = buff[0]; c[1] = buff[1];
        c[2] = buff[2]; c[3] = buff[3];
        // 若原先已收到一个位图消息，则清空原来的位图
        if (peer->bitmap.bitfield != NULL) {
            free(peer->bitmap.bitfield);
            peer->bitmap.bitfield = NULL;
        }
        peer->bitmap.valid_length = bitmap->valid_length;
        if (bitmap->bitfield_length != char_to_int(c)-1) {    // 若收到一个错误位图
            peer->state = CLOSING;
            // 丢弃发送缓冲区中的数据
            discard_send_buffle(peer);
            clear_btcache_before_peer_close(peer);
            peer->am_interested = is_interested(&(peer->bitmap), bitmap);
            close(peer->socket);
            return -1;
        }
        // 生成该peer的位图
        peer->bitmap.bitfield_length = char_to_int(c)-1;
        peer->bitmap.bitfield = (unsigned char*)malloc(peer->bitmap.bitfield_length);
        memcpy(peer->bitmap.bitfield, &buff[5], peer->bitmap.bitfield_length);

        // 如果原状态为已握手，收到位图后应该向peer发位图
        if (peer->state == HANDSHAKED) {
            create_bitfiled_msg(bitmap->bitfield, bitmap->bitfield_length,peer);
            peer->state = DATA;
        }
        // 如果原状态为已发送位图，收到位图后可以进入DATA状态准备交换数据
        if (peer->state == SENDBITFIELD) {
            peer->state = DATA;
        }
        // 根据位图判断peer是否对本客户端感兴趣
        peer->peer_interested = is_interested(bitmap, &(peer->bitmap));
        // 判断对peer是否感兴趣，若是则发送interested消息
        peer->am_interested = is_interested(&(peer->bitmap), bitmap);
        if (peer->am_interested == 1) create_chock_interested_msg(2, peer);
    }

    peer->state_timestamp = time(NULL);
    return 0;
}


