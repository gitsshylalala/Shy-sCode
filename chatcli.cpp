/*************************************************************************
	> File Name: chatcli.cpp
	> Author: 
	> Mail: 
	> Created Time: Fri 11 May 2018 04:03:11 PM CST
 ************************************************************************/

#include<list>
#include"pub.h"
#define FD_SIZE 64 
#define ERR_EXIT(x) do{\
                    perror(x);\
                    exit(0);\
                }while(0)
char clieID[16];

using namespace std;
//客户端也维护了一个聊天室成员链表
static list<user_info> client_list;

void do_someone_login(message& msg)
{
    user_info *user = (user_info*)msg.body;
    in_addr tmp;
    tmp.s_addr = user->ip;
    printf("user %s has already login,ip: %s port: %d !\n",user->username,inet_ntoa(tmp),ntohs(user->port));
    //将登陆用户加入列表
    client_list.push_back(*user);
}

void do_someone_logout(message& msg)
{
    //将登出从用户列表中移除
    list<user_info>::iterator it;
    for(it = client_list.begin();it != client_list.end();it++)
        if(strcmp(it->username,msg.body) == 0)
            break;

    if(it != client_list.end())
        client_list.erase(it);

    printf("user %s has logout!\n",msg.body);
}

void do_chat(message& msg)
{
    chat_msg *cm = (chat_msg*)msg.body;
    printf("recv a msg [%s] from [%s]\n",cm->msg,cm->username);
}

//参数:命令行 套接口 目标地址
void parse_cmd(char* cmdbuf,int sock,const struct sockaddr_in *addr){
    char cmd[10] = {0};
    char *p;
    int n = 0;

    //首先判断有没有空格
    p = strchr(cmdbuf,' ');//找到空格，定位到空格
    if(p != NULL)
        *p == '\0';

    strcpy(cmd,cmdbuf);

    if(strcmp(cmd,"exit") == 0)//退出选项
    {
        message climsg;
        memset(&climsg,0x00,sizeof(climsg));
        climsg.cmd = htonl(C2S_LOGOUT);
        memcpy(climsg.body,clieID,strlen(clieID) + 1); 

        if(sendto(sock,&climsg,sizeof(climsg),0,(struct sockaddr*)addr,sizeof(struct sockaddr)) == 0)
           ERR_EXIT("sendto");

        printf("user %s has logout server\n",clieID);

        client_list.clear();
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(cmd,"list") == 0)//列表选项
    {
        message climsg;
        memset(&climsg,0x00,sizeof(climsg));
        climsg.cmd = htonl(C2S_ONLINE_USER);

        if(sendto(sock,&climsg,sizeof(climsg),0,(struct sockaddr*)addr,sizeof(struct sockaddr)) == 0)
           ERR_EXIT("sendto");

        printf("Now List User_info List:\n");
        int getcount = 0;
FLAG2:
        n = recvfrom(sock , &getcount , sizeof(getcount) ,0, NULL,NULL);
        if(n < 0)//recvfrom失败
        {
            if(errno == EINTR)
               goto FLAG2;

            ERR_EXIT("recvfrom");
        }
        //成员个数
        int count = ntohl(getcount);
        
        int i = 0;
        for( i = 0; i < count; i++)
        {
            user_info user;
            memset(&user,0x00,sizeof(user));
FLAG3:
            n = recvfrom(sock, &user, sizeof(user), 0 ,NULL,NULL);
            if(n < 0)
            {
                if(errno == EINTR)
                    goto FLAG3;

                ERR_EXIT("recvfrom");
            }
            in_addr in;
            in.s_addr = user.ip;
            printf("No.%d user: %s ---ip: %s port: %d\n",i+1,user.username,inet_ntoa(in),ntohs(user.port));
        }
    }
    else if(strncmp(cmd,"send",4) == 0)//发送信息选项
    {
        char peer_name[16] = {0};
        char cmdcont[20] = {0};
        strcpy(cmdcont,cmd+5);
        char *space = strchr(cmdcont,' ');
        if(space == NULL)
            printf("Please write the cont of sendbuf!\n");
        else
        {
            //填装内容，确定用户名和发送内容
            *space = '\0';
            strcpy(peer_name,cmdcont);
            strcpy(cmdcont,space+1);
            printf("username: %s cont: %s \n",peer_name,cmdcont);

            if(strcmp(clieID,peer_name) == 0)
                printf("You can't send buf to yourself!\n");

            else
            {
                list<user_info>::iterator it = client_list.begin();
                for(it = client_list.begin();it != client_list.end();++it)
                {
                    if(strcmp(it->username,peer_name) == 0)
                        break;
                }
                
                //没找到
                if(it == client_list.end())
                    printf("The user is not in list!\n");

                else
                {
                    //发送消息
                    in_addr in;
                    in.s_addr = it->ip;
                    printf("send %s to %s --- ip: %s port: %d \n",cmdcont,peer_name,inet_ntoa(in),ntohs(it->port));

                    //根据it得到ip和port
                    struct sockaddr_in peeraddr;
                    memset(&peeraddr,0x00,sizeof(peeraddr));
                    peeraddr.sin_family = AF_INET;
                    peeraddr.sin_port = it->port;
                    peeraddr.sin_addr.s_addr = it->ip;

                    //发送命令报告 报告中加入内容
                    message msg;
                    memset(&msg,0x00,sizeof(msg));
                    chat_msg msg_c2c;
                    memset(&msg_c2c,0x00,sizeof(msg_c2c));

                    strcpy(msg_c2c.username,clieID);
                    strcpy(msg_c2c.msg,cmdcont);
                    msg.cmd = htonl(C2C_CHAT);
                    memcpy(msg.body,&msg_c2c,sizeof(msg_c2c));

                    if(sendto(sock,(const char*)&msg,sizeof(msg), 0 , (struct sockaddr*)&peeraddr,sizeof(struct sockaddr_in)) == 0)
                        ERR_EXIT("sendto");
                }
            } 
        }
    }
    else
    {
        printf("bad commands!Please write again!\n");
    }
}

void Tell_login(int sock,struct sockaddr_in* peeraddr)//通知登录
{
    message climsg;
    climsg.cmd = htonl(C2S_LOGIN);
    int n = 0;//表示接受的字符个数
    struct sockaddr_in clieaddr;

    //客户机地址
    memset(&clieaddr,0x00,sizeof(struct sockaddr_in));
    clieaddr.sin_family = AF_INET;
    clieaddr.sin_port = htons(5188);//端口，需要把主机字节序转换为网络字节序
    clieaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Please write down your username:");
    fflush(stdout);
    while(1){
        scanf("%s",clieID);
        memcpy(climsg.body,clieID,strlen(clieID)+1);

        if( (sendto(sock,&climsg,sizeof(climsg),0,(struct sockaddr*)&clieaddr,sizeof(struct sockaddr_in))) == 0)
            ERR_EXIT("sendto");

        //发送完后进入接受状态
        //分别接受
        //1.登录是否成功消息
        //2.登录如果成功则接受登录用户链表并打印
        message getmsg;
        memset(&getmsg,0x00,sizeof(getmsg));
        socklen_t peerlen = sizeof(struct sockaddr_in);

FLAG4:
        n =  recvfrom(sock , &getmsg , sizeof(getmsg) ,0, (struct sockaddr*)peeraddr,&peerlen);
        if(n < 0)
        {
            if(errno == EINTR)
                goto FLAG4;

            ERR_EXIT("recvfrom");
        }

        int cmd = ntohl(getmsg.cmd);
        if(cmd == S2C_ALREADY_LOGINED)
        {
            printf("user %s has already logined,please write down another name:",clieID);
            fflush(stdout);
        }
        else if(cmd == S2C_LOGIN_OK)
            break;
    }

    // cmd == S2C_LOGIN_OK 登录成功
    printf("Login Success!\n");

    printf("Now List User_info List:\n");
    int getcount = 0;
FLAG5:
    n = recvfrom(sock , &getcount , sizeof(getcount) ,0, NULL,NULL);
    if(n < 0)//recvfrom失败
    {
        if(errno == EINTR)
           goto FLAG5;

        ERR_EXIT("recvfrom");
    }
    //成员个数
    int count = ntohl(getcount);
    
    int i = 0;
    for( i = 0; i < count; i++)
    {
        user_info user;
        memset(&user,0x00,sizeof(user));
FLAG6:
        n = recvfrom(sock, &user, sizeof(user), 0 ,NULL,NULL);
        if(n < 0)
        {
            if(errno == EINTR)
                goto FLAG6;

            ERR_EXIT("recvfrom");
        }
        //将成员装入成员链表
        client_list.push_back(user);
        in_addr in;
        in.s_addr = user.ip;
        printf("No.%d user: %s ---ip: %s port: %d\n",i+1,user.username,inet_ntoa(in),ntohs(user.port));
    }
}

void chat_cli(int sock,const struct sockaddr_in* servaddr){
    printf("\n You can choice your commands:\n");
    printf("send [username] msg\n");
    printf("list\n");
    printf("exit\n");
    printf("\n");

    sockaddr_in peeraddr;
    socklen_t peerlen;
    message getmsg;
    int n = 0;

    //加入一个IO复用模型
    fd_set rset;
    FD_ZERO(&rset);
    int nread;
    while(1)
    {
        FD_SET(0,&rset);//标准输入装入集合
        FD_SET(sock,&rset);//sock套接口装入集合
        nread = select(sock+1,&rset,NULL,NULL,NULL);
        if(nread == -1)//errno
            ERR_EXIT("select");

        if(nread == 0)//并没有设置时间，所以不会超时
            continue;

        if(FD_ISSET(sock,&rset)) //S2C 或 C2C消息来了
        {
            peerlen = sizeof(peeraddr);
            memset(&getmsg,0x00,sizeof(getmsg));
        FLAG4:
            n = recvfrom(sock,&getmsg,sizeof(getmsg),0,(struct sockaddr*)&peeraddr,&peerlen);
            if(n < 0)
            {
                if(errno == EINTR)
                    goto FLAG4;

                ERR_EXIT("recvfrom");
            }
            int cmd = htonl(getmsg.cmd);
            switch(cmd){
                case S2C_SOMEONE_LOGIN:
                    do_someone_login(getmsg);
                    break;
                case S2C_SOMEONE_LOGOUT:
                    do_someone_logout(getmsg);
                    break;
                case C2C_CHAT:
                    do_chat(getmsg);
                    break;
                default:
                    break;
            }
            if((--nread) == 0)
                continue;
        }
        if(FD_ISSET(0,&rset))
        {
            char cmdbuf[100] = {0};
            if(fgets(cmdbuf,sizeof(cmdbuf),stdin) == NULL)
                break;

            if(cmdbuf[0] == '\n')
                continue;
            cmdbuf[strlen(cmdbuf) - 1] = '\0';
            parse_cmd(cmdbuf,sock,servaddr);
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

    //首先我登录后会给sock发送一个信息申明我登陆了
    Tell_login(sock,&servaddr);

    chat_cli(sock,&servaddr);

    close(sock);
    return 0;
}
