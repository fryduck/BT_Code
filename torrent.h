/*************************************************************************
	> File Name: torrent.h
	> Author: 
	> Mail: 
	> Created Time: Wed 04 May 2016 08:32:15 PM JST
 ************************************************************************/

#ifndef _TORRENT_H
#define _TORRENT_H
#include"tracker.h"

int download_upload_with_peer();        // 负责与所有peer收发数据，交换信息

int print_peer_list();                  // 打印peer链表中各个peer的IP和端口号
void print_process_info();              // 打印下载进度消息
void clear_connect_tracker();           // 释放与连接Tracker有关的一些的动态存储空间
void clear_connect_peer();              // 释放与连接peer有关的一些动态存储空间
void clear_tracker_response();          // 释放与解析Tracker回应有关的一些动态分配存储空间
void release_memory_in_torrent();       // 释放torrent.c中动态申请的存储空间
#endif
