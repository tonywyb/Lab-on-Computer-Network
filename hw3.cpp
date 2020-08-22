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

// calculate checksum 
unsigned short checkSum(char *pBuffer)
{
	int sum = 0;
	unsigned short result;
	for (int i = 0; i < 10; i++)
	{
		if (i != 5)
		{
			sum += (int)((unsigned char)pBuffer[2 * i] << 8);
			sum += (int)((unsigned char)pBuffer[2 * i + 1]);
		}
	}
	// transform to network-byte order(big endian)
	// keep add lower 16 bit and higher 16 bits until high 16 bits become 0 
	while ((sum & 0xffff0000) != 0)
		sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
	result = sum;
	// negate result
	return htons(~result);
}

// init routing table
void stud_Route_Init()
{
	routeTable.clear();
	return;
}

// update routing table
void stud_route_add(stud_route_msg *proute)
{
	int masklen = ntohl(proute->masklen);
	int dest = (ntohl(proute->dest)) & (0xffffffff << (32 - masklen));
	int next = ntohl(proute->nexthop);
	routeTable.insert(make_pair(dest, next));
	return;
}

// IPv4 packet routing and forwarding
int stud_fwd_deal(char *pBuffer, int length)
{
	int ttl = (int)pBuffer[8];
	int dest = ntohl(*(unsigned int *)(pBuffer + 16));
	// localhost is destination
	if (dest == getIpv4Address())
	{
		fwd_LocalRcv(pBuffer, length);
		return 0;
	}
	// use up Time-to-live, discard package
	if (ttl == 0)
	{
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
		return 1;
	}
	map<int, int>::iterator p = routeTable.find(dest);
	// entry found in routing table
	if (p != routeTable.end())
	{
		char *sendBuffer = new char[length];
		memcpy(sendBuffer, pBuffer, length);
		// decrement ttl 
		sendBuffer[8]--;			
		// need to re-calculate checkSum
		unsigned int sendSum = checkSum(sendBuffer);
		*((unsigned short *)(sendBuffer + 10)) = sendSum;
		fwd_SendtoLower(sendBuffer, length, p->second);
		return 0;
	}
	// discard package if entry is not found in routing table
	fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
	return 1;
}
