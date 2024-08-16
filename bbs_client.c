#include <stdio.h>
#include <sys/types.h>			/* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

int init_socket()
{
	int sockfd;
	struct sockaddr_in saddr;
	saddr.sin_family= AF_INET;
	saddr.sin_port= htons(2002);
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//1. 创建套接字
	sockfd = socket(AF_INET, SOCK_STREAM,0);
	//2. 连接服务器
	if(connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr)))
	{
		perror("connect");
		return -1;
	}
	return sockfd;
}

int main()
{
	int sockfd = init_socket();
	fd_set rdset;
	int r,n;
	char buf[1024];
	
	while(1)
	{
		FD_ZERO(&rdset);
		FD_SET(0, &rdset); 
		FD_SET(sockfd, &rdset);
	
		r = select(sockfd+1, &rdset, NULL, NULL, NULL);
		if(FD_ISSET(0, &rdset)) //键盘有数据
		{
			memset(buf,0,sizeof(buf));
			fgets(buf, sizeof(buf),stdin);
			send(sockfd, buf, strlen(buf)-1, 0);
			if(!strcmp(buf,"quit\n"))
			{
				close(sockfd);
				return 0;
			}
				
		}
		else if(FD_ISSET(sockfd, &rdset)) //网络有数据
		{
			memset(buf,0,sizeof(buf));
			n = recv(sockfd, buf,sizeof(buf),0 );
			printf("%s\n", buf);
		}
	}
	
}

