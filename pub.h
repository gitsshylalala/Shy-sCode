/*************************************************************************
	> File Name: pub.h
	> Author: 
	> Mail: 
	> Created Time: Fri 11 May 2018 03:52:37 PM CST
 ************************************************************************/

#ifndef _PUB_H
#define _PUB_H
#include <algorithm>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

//定义相关的宏
//C2S
#define C2S_LOGIN           0x01
#define C2S_LOGOUT          0x02
#define C2S_ONLINE_USER     0x03

#define MSG_LEN 512

//S2C
#define S2C_LOGIN_OK        0x01
#define S2C_ALREADY_LOGINED 0x02
#define S2C_SOMEONE_LOGIN   0x03
#define S2C_SOMEONE_LOGOUT  0x04
#define S2C_ONLINE_USER     0x05

//C2C
#define C2C_CHAT            0x06

struct message{//传递的消息结构
    int cmd;//网络字节序
    char body[MSG_LEN];//如果是登录消息，则存用户名
};

struct user_info{//用户信息
    char username[16];
    unsigned int ip;//四个字节 网络字节序
    unsigned short port;//两个字节 网络字节序
};

struct chat_msg{//C2C发送消息
    char username[16];
    char msg[100];
};

//公聊的方法：
//1.客户端给服务器端发消息，服务器端给其他用户发消息
//2.客户端本身维护了服务器端中的用户链表，直接和公聊对象构成连接。

#endif //_PUB_H
