/*************************************************************************
	> File Name: log.h
	> Author: 
	> Mail: 
	> Created Time: 2015年11月06日 星期五 23时21分09秒
 ************************************************************************/

#ifndef _LOG_H
#define _LOG_H

#include<stdarg.h>

//用于记录程序的行为
void logcmd(char *fmt,...);

//打开日志文件
int init_logfile(char *filename);

//将程序运行日志记录到文件
int logfile(char *file,int line,char *msg);

#endif
