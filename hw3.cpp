/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include<vector>
#include<algorithm>
#include<map>
using namespace std;

// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address();

typedef struct stud_route_msg
{
	unsigned int dest;
	unsigned int masklen;
	unsigned int nexthop;
} stud_route_msg;

map<int, int> routeTable;
// implemented by students

//计算校验和函数
unsigned short checkSum(char *pBuffer)
{
	int sum = 0;
	unsigned short result;
	for (int i = 0; i < 10; i++)
	{
		if (i != 5)	//校验和自己不加
		{
			sum += (int)((unsigned char)pBuffer[2 * i] << 8);
			sum += (int)((unsigned char)pBuffer[2 * i + 1]);
		}
	}
	//转换网络字节序
	while ((sum & 0xffff0000) != 0)
		sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
	//高16位不为0，反复将高16位与低16位相加
	result = sum;
	return htons(~result);
	//结果取反即为校验和
}

//初始化路由表
void stud_Route_Init()
{
	routeTable.clear();
	return;
}

//更新路由表
void stud_route_add(stud_route_msg *proute)
{
	int masklen = ntohl(proute->masklen);
	int dest = (ntohl(proute->dest)) & (0xffffffff << (32 - masklen));
	int next = ntohl(proute->nexthop);
	routeTable.insert(make_pair(dest, next));
	return;
}


int stud_fwd_deal(char *pBuffer, int length)
{
	int ttl = (int)pBuffer[8];
	int dest = ntohl(*(unsigned int *)(pBuffer + 16));
	if (dest == getIpv4Address())	//目的地是本机
	{
		fwd_LocalRcv(pBuffer, length);
		return 0;
	}
	if (ttl == 0)	//有效时间用完
	{
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
		return 1;
	}
	map<int, int>::iterator p = routeTable.find(dest);
	if (p != routeTable.end()) 	//找到相应表项
	{
		char *sendBuffer = new char[length];
		memcpy(sendBuffer, pBuffer, length);
		sendBuffer[8]--;			//TTL-1
		unsigned int sendSum = checkSum(sendBuffer);	//重新计算校验和
		*((unsigned short *)(sendBuffer + 10)) = sendSum;
		fwd_SendtoLower(sendBuffer, length, p->second);
		return 0;
	}
	//未找到相应表项
	fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
	return 1;
}
