#include "sysinclude.h"
#include <queue>
#include <vector>
#include <fstream>
#include <iostream>
using namespace std;
extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4

typedef enum {data, ack, nak} frame_kind;
typedef struct frame_head {
	frame_kind kind;
	unsigned int seq;
	unsigned int ack;
	unsigned char data[100];
};
typedef struct frame {
	frame_head head;
	unsigned int size;
};
typedef struct frame_cache {
	frame content;
	int size;
};
/*
* 停等协议测试函数
*/
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType)
{
	static queue<frame_cache> q;
	static int window = 0;
	if (messageType == MSG_TYPE_SEND) {
		frame_cache tmp;
		tmp.content = *(frame*)pBuffer;
		tmp.size = bufferSize;
		q.push(tmp);
		if (window < WINDOW_SIZE_STOP_WAIT) {
			SendFRAMEPacket((unsigned char*)pBuffer, (unsigned int)bufferSize);
			window++;
		}else {
			return 0;
		}

	}
	else if (messageType == MSG_TYPE_RECEIVE) {
		unsigned int received_ack = (((frame*)pBuffer)->head.ack);
		if (!q.empty()) {
			unsigned int saved_seq = q.front().content.head.seq;
			if (saved_seq == received_ack) {
				q.pop();

				if (!q.empty()) {
					SendFRAMEPacket((unsigned char*)(&q.front().content), 
						(unsigned int)(q.front().size));

				}else {
					window--;
				}
			}
		}
	}
	else if (messageType == MSG_TYPE_TIMEOUT) {
		// it must be the front frame
		SendFRAMEPacket((unsigned char*)(&q.front().content), (unsigned int)(q.front().size));		
	}

	return 0;
}

/*
* 回退n帧测试函数
*/
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType)
{
	static vector<frame_cache> wait_window;
	static vector<frame_cache> send_window;
	if (messageType == MSG_TYPE_SEND) {
		frame_cache tmp;
		tmp.content = *((frame*)pBuffer);
		tmp.size = bufferSize;
		if (send_window.size() < WINDOW_SIZE_BACK_N_FRAME) {
			send_window.push_back(tmp);
			SendFRAMEPacket((unsigned char*)pBuffer, (unsigned int)bufferSize);
		} else {
			wait_window.push_back(tmp);
		}
	} else if (messageType == MSG_TYPE_RECEIVE) {
		unsigned int received_ack = ((frame*)pBuffer)->head.ack;
		for (vector<frame_cache>::iterator it = send_window.begin(); it != send_window.end();it++) {
			unsigned int saved_seq = (*it).content.head.seq;
			send_window.erase(send_window.begin());
			if (received_ack == saved_seq) {
				break;
			}
		}
			

		while (send_window.size() < WINDOW_SIZE_BACK_N_FRAME && (wait_window.size() != 0)) {
			frame_cache tmp = wait_window[0];
			wait_window.erase(wait_window.begin());
			send_window.push_back(tmp);
			SendFRAMEPacket((unsigned char*)(&tmp.content), 
				(unsigned int)tmp.size);
			
		}
	

	} else if (messageType == MSG_TYPE_TIMEOUT) {
		for (vector<frame_cache>::iterator it = send_window.begin();it != send_window.end();it++) {
			SendFRAMEPacket((unsigned char*)(&((*it).content)),
				(unsigned int)((*it).size));
		}
	}
	return 0;
}

/*
* 选择性重传测试函数
*/
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType)
{
	static vector<frame_cache> wait_window;
	static vector<frame_cache> send_window;

	if (messageType == MSG_TYPE_SEND) {
		frame_cache tmp;
		tmp.content = *((frame*)pBuffer);
		tmp.size = bufferSize;
		if (send_window.size() < WINDOW_SIZE_BACK_N_FRAME) {
			send_window.push_back(tmp);
			SendFRAMEPacket((unsigned char*)pBuffer, (unsigned int)bufferSize);
		} else {
			wait_window.push_back(tmp);
		}
	}
	else if (messageType == MSG_TYPE_RECEIVE) {
		frame_kind k = ((frame*)pBuffer)->head.kind;
		k = (frame_kind)ntohl(k);
		if (k == ack) {
			unsigned int received_ack = ((frame*)pBuffer)->head.ack;
			for (int i = 0; i < send_window.size();i++) {
				unsigned int saved_seq = send_window[i].content.head.seq;
				send_window.erase(send_window.begin());
				if (received_ack == saved_seq) {
					break;
				}
			}
			while (send_window.size() < WINDOW_SIZE_BACK_N_FRAME && (wait_window.size() != 0)) {
				frame_cache tmp = wait_window[0];
				wait_window.erase(wait_window.begin());
				send_window.push_back(tmp);
				SendFRAMEPacket((unsigned char*)(&tmp.content), 
					(unsigned int)tmp.size);
				
			}
		}
		else if (k == nak){
			unsigned int wrong_seq = ((frame*)pBuffer)->head.ack;
			for (int i = 0;i < send_window.size();++i) {
				unsigned int saved_seq = send_window[i].content.head.seq;
				if (saved_seq == wrong_seq) {
					SendFRAMEPacket((unsigned char*)(& send_window[i].content),
						(unsigned int)send_window[i].size);
					break;
				}
			}
		}
	}
	
	return 0;
}

