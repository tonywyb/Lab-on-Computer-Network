#include <iostream>
/*
* THIS FILE IS FOR IP TEST
*/
// system support
#include "sysInclude.h"
using namespace std;

extern void ip_DiscardPkt(char* pBuffer,int type);

extern void ip_SendtoLower(char*pBuffer,int length);

extern void ip_SendtoUp(char *pBuffer,int length);

extern unsigned int getIpv4Address();

unsigned int checkSum(unsigned short *p, unsigned int size) 
{
	unsigned int ret = 0;
	while (size--) 
	{
		ret += ntohs(*p);
		p++;
	}
	while(ret >> 16) 
	{
		ret = (ret & 0xffff) + (ret >> 16);
	}
	return ret;
}
int stud_ip_recv(char *pBuffer,unsigned short length)
{
	// Version
	int version = ((*pBuffer) & 0xf0 ) >> 4;
	if (version != 4) 
	{
		ip_DiscardPkt(pBuffer ,STUD_IP_TEST_VERSION_ERROR);
		return 1;
	}
	// IP Head Length
	int HeaderLength = (*pBuffer & 0xf);
	if (HeaderLength != 5) 
	{
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
		return 1;
	}
	// Time to Live
	int TTL = *(pBuffer + 8);
	if (TTL == 0) 
	{
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
		return 1;
	}
	// Header checksum
	unsigned short checksumIn = ntohs(*((short*)pBuffer + 5));
	*((short*)pBuffer+5) = 0;
	unsigned short checksumCalc = ~checkSum((unsigned short*)pBuffer, HeaderLength*2);
	if (checksumIn != checksumCalc) 
	{
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
		return 1;
	}
	// Destination address
	unsigned int destinationAddress = ntohl(*((unsigned int*)pBuffer + 4));
	unsigned int hostAddress = getIpv4Address();
	if (destinationAddress != hostAddress && destinationAddress != ~0) 
	{
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
		return 1;
	}
	ip_SendtoUp(pBuffer + 20, length - 20);
	return 0;
}

int stud_ip_Upsend(char *pBuffer,unsigned short len,unsigned int srcAddr,
				   unsigned int dstAddr,byte protocol,byte ttl)
{
	char* nBuffer = new char[20 + len];
	memset(nBuffer, 0, 20 + len);
	//version & IHL
	*nBuffer = (4 << 4) | 5;
	unsigned short * pShort = (unsigned short*)nBuffer;
	unsigned int * pInt = (unsigned int*)nBuffer;
	//total length
	*(pShort + 1) = htons(20 + len);
	//source address
	*(pInt + 3) = htonl(srcAddr);
	//destination address
	*(pInt + 4) = htonl(dstAddr);
	// time to live
	*(nBuffer + 8) = ttl;
	//protocol
	*(nBuffer + 9) = protocol;
	//check sum
	unsigned short checksum = checkSum((unsigned short*)nBuffer, 10);
	*(pShort + 5) = htons(~checksum);
	memcpy(nBuffer + 20, pBuffer, len);
	//append header
	ip_SendtoLower(nBuffer, len + 20);
	return 0;
}

