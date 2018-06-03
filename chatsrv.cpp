/*************************************************************************
	> File Name: chatsrv.cpp
	> Author: 
	> Mail: 
	> Created Time: Fri 11 May 2018 04:03:22 PM CST
 ************************************************************************/
//服务器端。
#include <list>
#include"pub.h"
#define FD_SIZE 64 
#define ERR_EXIT(x) do{\
                    perror(x);\
                    exit(0);\
                }while(0)

using namespace std;
//全局变量
static list<user_info> client_list;

//用户登录 消息里放用户名
void do_clilogin(const message& msg, int sock,struct sockaddr_in* addr)
{
    //将登录用户信息装入USER_LIST中
    //并且告诉所有用户有人登录
    user_info user;
    strcpy(user.username,msg.body);
    user.ip = addr->sin_addr.s_addr;//网络字节序
    user.port = addr->sin_port;//网络字节序

    /*查找用户*/
    list<user_info>::iterator it;//迭代器
    for(it = client_list.begin();it != client_list.end();it++)
    {
        if(strcmp(it->username,msg.body) == 0)
        {
            break;
        }
    }

    if(it == client_list.end())//没找到
    {
        //保存
        printf("%s has login! addr:%s,port:%d \n",msg.body,inet_ntoa(addr->sin_addr),ntohs(addr->sin_port));
        client_list.push_back(user);

        //回复
        message reply_msg;
        memset(&reply_msg,0x00,sizeof(reply_msg));
        reply_msg.cmd = htonl(S2C_LOGIN_OK);
        if( (sendto(sock,&reply_msg,sizeof(reply_msg),0,(struct sockaddr*)addr,sizeof(struct sockaddr_in))) == 0)
            ERR_EXIT("sendto");

        //发送在线人数
        int count = (int)client_list.size();
        int send_count = htonl(count);
        if( (sendto(sock,&send_count,sizeof(int),0,(struct sockaddr*)addr,sizeof(struct sockaddr_in))) == 0)
            ERR_EXIT("sendto");
        printf("sending user list information to:%s,addr:%s,port:%d\n",user.username,inet_ntoa(addr->sin_addr),ntohs(user.port));

        //发送在线列表
        for(it = client_list.begin(); it != client_list.end(); it++)
        {
            user_info user = *it;
            if( (sendto(sock,&user,sizeof(user_info),0,(struct sockaddr*)addr,sizeof(struct sockaddr_in))) == 0)
                ERR_EXIT("sendto");
        }

        //广播 对每一个用户地址和端口发消息
        for(it = client_list.begin(); it != client_list.end(); it++)
        {
            if(strcmp(it->username,user.username) == 0)//不用给自己通知
                continue;

            message ToAllMsg;
            sockaddr_in peeraddr;
        
            ToAllMsg.cmd = htonl(S2C_SOMEONE_LOGIN);//转换成网络字节序
            //将当前登录者user信息传过去
            memcpy(ToAllMsg.body,&user,sizeof(user));
            memset(&peeraddr,0x00,sizeof(peeraddr));
            peeraddr.sin_family = AF_INET;
            peeraddr.sin_port = it->port;//端口，需要把主机字节序转换为网络字节序
            peeraddr.sin_addr.s_addr = it->ip;

            if((sendto(sock,&ToAllMsg,sizeof(ToAllMsg),0,(struct sockaddr*)&peeraddr,sizeof(peeraddr))) == 0)
                ERR_EXIT("sendto");
        }
    }

    else //找到用户
    {
        printf("user %s has already login!\n",user.username);

        //回复
        message reply_msg;
        memset(&reply_msg,0x00,sizeof(reply_msg));
        reply_msg.cmd = htonl(S2C_ALREADY_LOGINED);
        if((sendto(sock,&reply_msg,sizeof(reply_msg),0,(struct sockaddr*)addr,sizeof(struct sockaddr_in))) == 0)
            ERR_EXIT("sendto");
    }
}

//用户登出
void do_clilogout(const message &msg,int sock,struct sockaddr_in* addr)
{
    //知道有人登出后声明
    printf("%s user has logout,addr:%s,port:%d \n",msg.body,inet_ntoa(addr->sin_addr),ntohs(addr->sin_port));

    list<user_info>::iterator it;//迭代器
    //找到对应用户并移出
    for(it = client_list.begin();it != client_list.end(); it++)
    {
        if(strcmp(it->username,msg.body) == 0)
            break;
    }
    if(it != client_list.end())
        client_list.erase(it);
    else
        return;
    
    //广播
    message ToAllMsg; 
    ToAllMsg.cmd = htonl(S2C_SOMEONE_LOGOUT);
    strcpy(ToAllMsg.body,msg.body);

    for(it = client_list.begin();it != client_list.end(); it++)
    {
        if(strcmp(it->username,msg.body) == 0)
            continue;

        struct sockaddr_in peeraddr;
        peeraddr.sin_family = AF_INET;
        peeraddr.sin_port = it->port;//端口，需要把主机字节序转换为网络字节序
        peeraddr.sin_addr.s_addr = it->ip;

        if( (sendto(sock,&ToAllMsg,sizeof(ToAllMsg),0,(struct sockaddr*)&peeraddr,sizeof(peeraddr))) == 0)
            ERR_EXIT("sendto");
    }
}

//用户请求查看列表
void do_clilist(int sock,struct sockaddr_in* addr)
{
    //现在接收到命令需求为list。
    //则需要根据请求服务的客户机地址。
    //回复给客户关于链表中的内容。

    list<user_info>::iterator it;//迭代器

    //发送在线人数
    int count = (int)client_list.size();
    int send_count = htonl(count);
    if( (sendto(sock,&send_count,sizeof(int),0,(struct sockaddr*)addr,sizeof(struct sockaddr_in))) == 0)
         ERR_EXIT("sendto");

    //发送在线列表
    for(it = client_list.begin(); it != client_list.end(); it++)
    {
        user_info user = *it;
        if((sendto(sock,&user,sizeof(user_info),0,(struct sockaddr*)addr,sizeof(struct sockaddr_in))) == 0)
            ERR_EXIT("sendto");
    }
}

//服务器端服务函数
void chat_srv(int sock)
{
    while(1)
    {
        //首先要用一个消息包去接收消息，
        struct sockaddr_in  clientaddr;
        socklen_t           clientlen;
        int                 n;
        message             msg;

        memset(&msg,0x00,sizeof(msg));
        clientlen = sizeof(clientaddr);
        n = recvfrom(sock , &msg , sizeof(msg) , 0 , (struct sockaddr*)&clientaddr , &clientlen);
        if(n < 0)//判错
        {
            if(errno == EINTR)
                continue;

            ERR_EXIT("recvfrom");
        }

        int cmd = ntohl(msg.cmd);//因为传过来的时候是以网络字节序传来的,所以要做转换
        switch(cmd)
        {
            case C2S_LOGIN:
            do_clilogin(msg,sock,&clientaddr);
            break;
            case C2S_LOGOUT:
            do_clilogout(msg,sock,&clientaddr);
            break;
            case C2S_ONLINE_USER:
            do_clilist(sock,&clientaddr);
            break;
            default:
            break;
        }
    }
}

int main(void)
{
    int sock;

    /*socket*/
    if((sock = socket(PF_INET,SOCK_DGRAM,0)) < 0) 
         ERR_EXIT("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr,0x00,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(5188);//端口，需要把主机字节序转换为网络字节序
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //绑定
    /*bind*/
    if(bind(sock,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0)
        ERR_EXIT("bind");

    chat_srv(sock);

    close(sock);

    return 0;
}
