/*************************************************************************
	> File Name: message.h
	> Author: 
	> Mail: 
	> Created Time: 2015年11月11日 星期三 22时14分00秒
 ************************************************************************/

#ifndef _MESSAGE_H
#define _MESSAGE_H

#include"peer.h"


int int_to_char(int i,unsigned char c[4]);        // 将整型变量i的4个字节存放到数组c中
int char_to_int(unsigned char c[4]);              // 将数组c中的4个字节转换为一个整型数

// 以下函数创建各个类型的消息，创建消息的函数请参考BT协议加以理解
int create_handshake_msg(char *info_hash,char *peer_id,Peer *peer);
int create_keep_alive_msg(Peer *peer);
int Create_chock_interested_msg(int type,Peer *peer);
int create_have_msg(int index,Peer *peer);
int create_bitfield_msg(char *bitfield,int bitfield_len,Peer *peer);
int create_request_msg(int index,int begin,int length,Peer *peer);
int create_piece_msg(int index,int begin,char *block,int b_len,Peer *peer);
int create_cancel_msg(int index,int begin,int length,Peer *peer);
int create_port_msg(int port,Peer *peer);

// 判断接收缓冲区中是否存放了一条完整的消息
int is_complete_message(unsigned char *buff,unsigned int len,int *ok_len);
// 处理收到的消息，接收缓冲区中存放着一条完整的消息
int parse_reponse(Peer *peer);
// 处理收到的消息，接收缓冲区除了存放着一条完整的消息外，还有其他不完整的消息
int parse_response_uncomplete_msg(Peer *p,int ok_len);
// 根据当前的状态创建响应消息
int create_response_message(Peer *peer);
// 为发送have消息作准备，have消息较为特殊，它要发送给所有peer
int prepare_send_have_msg();
// 即将与peer断开时，丢弃套接字发送缓冲区中的所有未发送的消息
void discard_send_buffer(Peer *peer);

#endif
