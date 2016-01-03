/*************************************************************************
	> File Name: signal_hander.h
	> Author: 
	> Mail: 
	> Created Time: 2015年11月08日 星期日 22时37分15秒
 ************************************************************************/

#ifndef _SIGNAL_HANDER_H
#define _SIGNAL_HANDER_H

//做一些清理工作，如释放动态分配的内存
void do_clear_work();
//处理一些信号
void process_signal(int signo);
//设置信号处理函数
int set_signal_hander();

#endif
