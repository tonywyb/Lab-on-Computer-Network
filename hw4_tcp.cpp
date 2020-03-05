/*
* THIS FILE IS FOR TCP TEST
*/
#include "sysInclude.h"
#include<cstring>
#include<fstream>


extern void tcp_DiscardPkt(char *pBuffer, int type);

extern void tcp_sendReport(int type);

extern void tcp_sendIpPkt(unsigned char *pData, UINT16 len, unsigned int  srcAddr, unsigned int dstAddr, UINT8  ttl);

extern int waitIpPacket(char *pBuffer, int timeout);

extern unsigned int getIpv4Address();

extern unsigned int getServerIpv4Address();

// 定义TCPhead结构体，方便操作
typedef struct TCPhead
{
	unsigned short srcPort;
	unsigned short dstPort;
	unsigned int seqNum;
	unsigned int ackNum;
	unsigned char  headLen;
	unsigned char  flag;
	unsigned short winSize;
	unsigned short checksum;
	unsigned short urgPtr;
	char data[100];
};

// 本地的TCB状态信息，TCB里所有数据均为小端序
typedef struct TCB
{
	unsigned int srcAddr;
	unsigned int dstAddr;
	unsigned short srcPort;
	unsigned short dstPort;
	unsigned int seq;
	unsigned int ack;
	int sockfd;
	unsigned char state;
	unsigned char* data;
};


//TCB按链表形式组织
typedef struct TCBnode
{
	TCB *current;
	struct TCBnode *next;
};

// TCB链表与当前操作的TCB
struct TCBnode *TCBlist;
struct TCB *currentTCB;

// 定义TCP连接的有限状态机
enum status
{
	CLOSED,
	SYN_SENT,
	ESTABLISHED,
	FIN_WAIT1,
	FIN_WAIT2,
	TIME_WAIT
};

//全局变量
int gSrcPort = 2005;
int gDstPort = 2006;
int gSeqNum = 1;
int gAckNum = 0;
int socknum = 5;


//统一按小端法处理，基于之前实验的函数，对服务器的捎带确认做了点处理
unsigned int getchecksum(char *pBuffer, unsigned int srcAddr, unsigned int dstAddr, unsigned short len, char* data)
{
	TCPhead *head = (TCPhead *)pBuffer;
	int flag = 0;
	if (head->flag != PACKET_TYPE_DATA && len > 20)
	{
		flag = 1;
		len -= 20;
	}
	unsigned int sum = 0;
	unsigned short result;
	//伪首部
	sum += (srcAddr >> 16) + (srcAddr & 0xffff);
	sum += (dstAddr >> 16) + (dstAddr & 0xffff);
	sum += 6;		//传输协议号
	sum += 0x14;	//长度
	//真头部
	for (int i = 0; i < 10; i++)
	{
		if (i != 8)
		{
			sum += (int)((unsigned char)pBuffer[2 * i] << 8);
			sum += (int)((unsigned char)pBuffer[2 * i + 1]);
		}
	}
	//对TCP数据进行校验
	if (head->flag == PACKET_TYPE_DATA || flag == 1)
	{
		sum += len;
		int length = len;
		char *p;
		if (data != NULL)
			p = data;
		else
			p = head->data;
		while (length > 0)
		{
			sum += (*p) << 8;
			p++;
			length--;
			if (length > 0)
			{
				sum += (*p);
				p++;
				length--;
			}
		}
	}
	//高16位不为0，反复将高16位与低16位相加
	while ((sum & 0xffff0000) != 0)
		sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
	result = sum;
	//结果取反即为校验和
	return htons(~result);
}

/*
* 按照sockfd寻找TCB
*/
int getSockfd(int sockfd)
{
	TCBnode *p = TCBlist;
	while (p != NULL && p->current != NULL)
	{
		if (p->current->sockfd == sockfd)
		{
			currentTCB = p->current;
			return 0;
		}
		p = p->next;
	}
	return -1;
}

/*
* 接收tcp分组的函数，srcAddr和dstAddr为大端序
*/
int stud_tcp_input(char *pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr)
{
	TCPhead *header = (TCPhead *)pBuffer;
	unsigned short checksum = header->checksum;

	if (checksum != getchecksum(pBuffer, ntohl(srcAddr), ntohl(dstAddr), len, NULL))
		return -1;

	header->ackNum = ntohl(header->ackNum);
	header->seqNum = ntohl(header->seqNum);
	int seqAdd = 1;
	if (currentTCB->state == FIN_WAIT2)
		seqAdd = 0;
	else if (len > 20)
		seqAdd = len - 20;
	//检查ack与seq是否匹配
	if (header->ackNum != (currentTCB->seq + seqAdd))
	{
		tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
		return -1;
	}
	//按照TCB的state更新状态
	switch (currentTCB->state)
	{
	case SYN_SENT:
		if (header->flag == PACKET_TYPE_SYN_ACK)
		{
			currentTCB->state = ESTABLISHED;
			currentTCB->ack = header->seqNum + 1;
			currentTCB->seq = header->ackNum;
			stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, currentTCB->srcPort, currentTCB->dstPort, ntohl(dstAddr), ntohl(srcAddr));
			break;
		}
		else
			return -1;
	case ESTABLISHED:
		if (header->flag == PACKET_TYPE_ACK)
		{
			if (len > 20)
			{
				currentTCB->ack = header->seqNum + len - 20;
				currentTCB->seq = header->ackNum;
				break;
			}
			else if (len == 20)
			{
				currentTCB->ack = header->seqNum + 1;
				currentTCB->seq = header->ackNum;
				break;
			}
			else
				return -1;
		}
		else
			return -1;
	case FIN_WAIT1:
		if (header->flag == PACKET_TYPE_ACK)
		{
			currentTCB->ack = header->seqNum + 1;
			currentTCB->seq = header->ackNum;
			currentTCB->state = FIN_WAIT2;
			break;
		}
		else
			return -1;
	case FIN_WAIT2:
		if (header->flag == PACKET_TYPE_FIN_ACK)
		{
			currentTCB->state = TIME_WAIT;
			stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, currentTCB->srcPort, currentTCB->dstPort, ntohl(dstAddr), ntohl(srcAddr));
			break;
		}
		else
			return -1;
	default:
		return -1;
	}
	return 0;
}

/*
* 封装分组并发送，两个地址为小端序
*/
void stud_tcp_output(char *pData, unsigned short len, unsigned char flag, unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr)
{
	if (currentTCB == NULL)
	{
		currentTCB = new TCB;
		currentTCB->seq = gSeqNum;
		currentTCB->ack = gAckNum;
		currentTCB->srcPort = srcPort;
		currentTCB->dstPort = dstPort;
		currentTCB->srcAddr = ntohl(srcAddr);
		currentTCB->dstAddr = ntohl(dstAddr);
		currentTCB->state = CLOSED;
	}
	switch (currentTCB->state)
	{
	case CLOSED:
		if (flag == PACKET_TYPE_SYN)
			currentTCB->state = SYN_SENT;
		else
			return;
		break;
	case ESTABLISHED:
		if (flag == PACKET_TYPE_FIN_ACK)
		{
			currentTCB->state = FIN_WAIT1;
			break;
		}
		else if (flag == PACKET_TYPE_DATA)
			break;
		else if (flag == PACKET_TYPE_ACK)
			break;
		else
			return;
		break;
	}

	TCPhead *newHead = new TCPhead;
	for (int i = 0; i < len; i++)
		newHead->data[i] = pData[i];
	//转换字节序并计算校验和
	newHead->srcPort = htons(srcPort);
	newHead->dstPort = htons(dstPort);
	newHead->seqNum = htonl(currentTCB->seq);
	newHead->ackNum = htonl(currentTCB->ack);
	newHead->headLen = 0x50;
	newHead->flag = flag;
	newHead->winSize = htons(1);
	newHead->urgPtr = htons(0);
	newHead->checksum = getchecksum((char *)newHead, srcAddr, dstAddr, len, pData);
	tcp_sendIpPkt((unsigned char *)newHead, 20 + len, srcAddr, dstAddr, 60);
}

/*
* 初始化socket，创建新TCB，分配sockfd等
*/
int stud_tcp_socket(int domain, int type, int protocol)
{
	if (domain != AF_INET || type != SOCK_STREAM || protocol != IPPROTO_TCP)	//检查参数是否合法
		return -1;
	currentTCB = new TCB;
	if (TCBlist == NULL)
	{
		TCBlist = new TCBnode;
		TCBlist->current = currentTCB;
		TCBlist->next = NULL;
	}
	else
	{
		TCBnode *temp = TCBlist;
		while (temp->next != NULL)
			temp = temp->next;
		temp->next = new TCBnode;
		temp->next->current = currentTCB;
		temp->next->next = NULL;
	}
	currentTCB->sockfd = socknum++;
	currentTCB->srcPort = gSrcPort++;
	currentTCB->seq = gSeqNum++;
	currentTCB->ack = gAckNum;
	currentTCB->state = CLOSED;

	return currentTCB->sockfd;
}

/*
* 创建连接，三次握手
*/
int stud_tcp_connect(int sockfd, struct sockaddr_in *addr, int addrlen)
{
	if (getSockfd(sockfd) == -1)
		return -1;

	unsigned int srcAddr = getIpv4Address();
	unsigned int dstAddr = ntohl(addr->sin_addr.s_addr);
	currentTCB->srcAddr = srcAddr;
	currentTCB->dstAddr = dstAddr;
	currentTCB->dstPort = ntohs(addr->sin_port);
	currentTCB->state = SYN_SENT;

	stud_tcp_output(NULL, 0, PACKET_TYPE_SYN, currentTCB->srcPort, currentTCB->dstPort, srcAddr, dstAddr);

	TCPhead *receive = new TCPhead;
	int res = -1;
	while (res == -1)
	{
		res = waitIpPacket((char*)receive, 5000);
	}
	stud_tcp_input((char *)receive, 20, htonl(currentTCB->dstAddr), htonl(currentTCB->srcAddr));

	return 0;
}

/*
* 发送数据，等待ACK
* */
int stud_tcp_send(int sockfd, const unsigned char *pData, unsigned short datalen, int flags)
{
	if (getSockfd(sockfd) == -1)
		return -1;
	if (currentTCB->state != ESTABLISHED)
		return -1;
	unsigned int srcAddr = currentTCB->srcAddr;
	unsigned int dstAddr = currentTCB->dstAddr;

	currentTCB->data = new unsigned char(datalen);
	memcpy((char *)currentTCB->data, (char *)pData, datalen);

	stud_tcp_output((char *)currentTCB->data, datalen, PACKET_TYPE_DATA, currentTCB->srcPort, currentTCB->dstPort, srcAddr, dstAddr);

	TCPhead *receive = new TCPhead;
	int res = -1;
	while (res == -1)
		res = waitIpPacket((char *)receive, 5000);

	stud_tcp_input((char *)receive, datalen + 20, srcAddr, dstAddr);

	return 0;
}

/*
* 接收数据，要发送ACK
* */
int stud_tcp_recv(int sockfd, unsigned char *pData, unsigned short datalen, int flags)
{
	if (getSockfd(sockfd) == -1)
		return -1;
	if (currentTCB->state != ESTABLISHED)
		return -1;

	unsigned int srcAddr = currentTCB->srcAddr;
	unsigned int dstAddr = currentTCB->dstAddr;

	TCPhead *receive = new TCPhead;
	int res = -1;
	while (res == -1)
		res = waitIpPacket((char *)receive, 5000);

	strcpy((char *)pData, (char *)receive->data);
	datalen = sizeof(pData);

	stud_tcp_input((char *)receive, datalen + 20, htonl(dstAddr), htonl(srcAddr));

	stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, currentTCB->srcPort, currentTCB->dstPort, srcAddr, dstAddr);

	return 0;
}

/*
* 关闭连接，状态逐步转换
* */
int stud_tcp_close(int sockfd)
{
	TCBnode *temp = TCBlist;
	TCBnode *pre = temp;
	while (temp != NULL && temp->current != NULL)
	{
		if (temp->current->sockfd == sockfd)
		{
			currentTCB = temp->current;
			break;
		}
		pre = temp;
		temp = temp->next;
	}

	if (temp == NULL)
		return -1;

	unsigned int srcAddr = htonl(currentTCB->srcAddr);
	unsigned int dstAddr = htonl(currentTCB->dstAddr);

	if (currentTCB->state != ESTABLISHED)
	{
		if (temp != pre)
		{
			pre->next = temp->next;
			delete temp;
		}
		else
			delete currentTCB;
		currentTCB = NULL;
		return -1;
	}
	else
	{
		stud_tcp_output(NULL, 0, PACKET_TYPE_FIN_ACK, currentTCB->srcPort, currentTCB->dstPort, ntohl(srcAddr), ntohl(dstAddr));

		TCPhead* receive = new TCPhead;
		int res = -1;
		while (res == -1)
			res = waitIpPacket((char*)receive, 5000);
		stud_tcp_input((char *)receive, 20, dstAddr, srcAddr);

		res = -1;
		while (res == -1)
			res = waitIpPacket((char*)receive, 5000);
		stud_tcp_input((char *)receive, 20, dstAddr, srcAddr);
	}
	return 0;
}
