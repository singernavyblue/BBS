#include <stdio.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

pthread_rwlock_t lockList = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockFile = PTHREAD_RWLOCK_INITIALIZER;
// 类型信息
typedef enum ClientType
{
	NEWS_TYPE,
	ENTERTAINMENT_TYPE,
	SUPPLY_TYPE
} ClientType;

// 客户端节点
typedef struct ClientNode
{
	int sockfd;
	enum ClientType type; // 客户类型
	char logname[20];	  // 登录名
	struct ClientNode *next;
} ClientNode;

ClientNode *head = NULL;

// 将客户端节点插入链表中
void insertNode(ClientNode *node)
{
	pthread_rwlock_wrlock(&lockList);
	node->next = head;
	head = node;
	pthread_rwlock_unlock(&lockList);
}

void SendHistory(char *filename, int connfd)
{
	FILE *file = fopen(filename, "r");
	char buf[200];
	if (file == NULL)
	{
		perror("open error");
	}
	while (fgets(buf, sizeof(buf), file))
	{
		write(connfd, buf, strlen(buf));
	}
	fclose(file);
}

void deleteNode(ClientNode *p)
{
	if (head = p)
	{
		head = head->next;
	}
	else
	{
		ClientNode *s = head;
		while (s->next != NULL)
		{
			s = s->next;
			if (s->next == p)
			{
				s->next = p->next;
			}
		}
		free(p);
	}
}
void save2File(char *filename, char *buf)
{
	FILE *file = fopen(filename, "a");
	if (file == NULL)
	{
		perror("open error");
	}
	fprintf(file, "%s", buf);
	fclose(file);
}

void multicastMsg(ClientNode *p, char *buf)
{
	ClientNode *s = head;
	while (s != NULL)
	{
		if (s != p && s->type == p->type)
		{
			write(s->sockfd, buf, strlen(buf));
		}
		s = s->next;
	}
}

char *menu = "\n"
			 "--------------------\n"
			 "-----登录类型选择---\n"
			 "-------1.新闻-------\n"
			 "-------2.娱乐-------\n"
			 "-------3.交易-------\n"
			 "--------------------\n"
			 "请选择登录的类别----\n";

int init_server()
{
	int sockfd;
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(2002);
	saddr.sin_addr.s_addr = INADDR_ANY; // 由内核来分配一个合适网卡IP
	// 1. 创建套接字
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// 设置sockfd可以重复使用port
	int on = 1;
	int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	printf("ret = %d\n", ret);
	// 2. bind
	if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)))
	{
		perror("bind");
		return -1;
	}
	// 3. listen
	listen(sockfd, 10);
	return sockfd;
}

void *talk2client(void *arg)
{
	int connfd = *(int *)arg;
	int result;
	char buf[200];
	char *filename;

	printf("confd = %d\n", connfd);
	// 分配一个新的节点Node
	ClientNode *p = (ClientNode *)malloc(sizeof(ClientNode));
	// 1. 填入 sockfd
	p->sockfd = connfd;
	p->next = NULL;
	// 2. 读登录名
	write(connfd, "请输入登录名", strlen("请输入登录名"));
	memset(buf, 0, sizeof(buf));
	result = read(connfd, buf, sizeof(buf));
	printf("buf=%s\n", buf);
	strcpy(p->logname, buf);
	// 3.登录的类型
	write(connfd, menu, strlen(menu));
	memset(buf, 0, sizeof(buf));
	result = read(connfd, buf, sizeof(buf));

	if (result <= 0)
	{
		printf("客户端断开连接\n");
		close(connfd);
		pthread_exit(NULL);
	}
	printf("buf=%s\n", buf);
	if (buf[0] == '1')
	{
		p->type = NEWS_TYPE;
		filename = "bbs_news.txt";
	}
	else if (buf[0] == '2')
	{
		p->type = ENTERTAINMENT_TYPE;
		filename = "bbs_fun.txt";
	}
	else if (buf[0] == '3')
	{
		p->type = SUPPLY_TYPE;
		filename = "bbs_trans.txt";
	}

	// 插入客户节点到 head 链表
	pthread_rwlock_wrlock(&lockList);
	insertNode(p);
	pthread_rwlock_unlock(&lockList);
	// 发送历史记录
	pthread_rwlock_rdlock(&lockFile);
	SendHistory(filename, connfd);
	pthread_rwlock_unlock(&lockFile);

	// 进入主循环
	while (1)
	{
		// 等待客户发布消息
		result = recv(connfd, buf, sizeof(buf), 0);
		if (result < 0)
		{
			perror("recv");
			continue;
		}
		else if (result == 0)
		{
			printf("客户端断开连接\n");
			pthread_rwlock_wrlock(&lockList);
			deleteNode(p);
			pthread_rwlock_unlock(&lockList);
			break;
		}
		else
		{
			if (strncmp(buf, "exit\n", 5) == 0)
			{
				printf("客户端主动离开\n");
				pthread_rwlock_wrlock(&lockList);
				deleteNode(p);
				pthread_rwlock_unlock(&lockList);
				break;
			}
			// 保持记录到 对应的文件
			pthread_rwlock_wrlock(&lockFile);
			save2File(filename, buf);
			pthread_rwlock_unlock(&lockFile);
			// 多播信息
			multicastMsg(p, buf);
		}
	}
}

int main()
{
	struct sockaddr_in peeraddr;
	pthread_t pthread1;

	int addrlen = sizeof(peeraddr);
	int sockfd = init_server();
	int connfd;
	while (1)
	{
		connfd = accept(sockfd, (struct sockaddr *)&peeraddr, &addrlen);
		printf(
			"%s connect server\n", inet_ntoa(peeraddr.sin_addr));
		pthread_create(&pthread1, NULL, talk2client, &connfd);
	}
}
