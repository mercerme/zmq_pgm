#include <future>
#include <iostream>
#include <string>
#include "zmq.hpp"
#include "zmq_addon.hpp"
#include <fstream>
#include <iomanip>
#include <windows.h>
#include <time.h>
#include "da_marketdata.h"

uint64_t xdk_nanosecond_timestamp() { //用来获取纳秒级别的时间戳
	static uint64_t baseline = 0;
	static uint64_t multiplier = 0;
	static LARGE_INTEGER frequency;
	static LARGE_INTEGER base_counter;

	uint64_t timestamp = 0;
	if (baseline == 0) {
		::QueryPerformanceFrequency(&frequency);
		multiplier = 1000000000ULL / frequency.QuadPart;
		FILETIME file_time;
		::GetSystemTimeAsFileTime(&file_time);
		baseline |= file_time.dwHighDateTime;
		baseline <<= 32;
		baseline |= file_time.dwLowDateTime;
		baseline *= 100;
		baseline -= 11644473600000000000ULL;
		::QueryPerformanceCounter(&base_counter);
		timestamp = baseline;
	}
	else {
		LARGE_INTEGER counter;
		::QueryPerformanceCounter(&counter);
		timestamp = baseline + (counter.QuadPart - base_counter.QuadPart) * multiplier;
	}
	return timestamp;
}

inline void xdk_microsecond_delay(uint64_t interval) { //用来进行纳秒级别的sleep
	interval *= 1000ULL;
	uint64_t t, t0 = xdk_nanosecond_timestamp();
	do
	{
		t = xdk_nanosecond_timestamp();
	} while (t - t0 < interval);
}

void EPGMSubscriberThread() {
	void* context = zmq_ctx_new();                       //创建一个新的上下文
	void* subscriber = zmq_socket(context, ZMQ_SUB);     //创建一个新的zmq的socket 类型为sub
	zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);    //sub类型的socket要设置过滤器，第三项为数据开头的报文，第四项为第三项的数据大小，若设置为""和0即收取所有
	int rate = 1250000;
	zmq_setsockopt(subscriber, ZMQ_RATE, &rate, sizeof(rate)); //设置socket的接收速率，单位为kilobits per second
	int rcvbuf = 20000000;
	zmq_setsockopt(subscriber, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));//设置socket的接受区buffer, 单位为bytes
	int rcvhwm = 9999999;
	zmq_setsockopt(subscriber, ZMQ_RCVHWM, &rcvhwm, sizeof(rcvhwm)); //设置socket的接收区高水位，如果管道内堆积的数据超过水位，新进来的数据将直接被管道drop
	int recover = 60000;
	zmq_setsockopt(subscriber, ZMQ_RECOVERY_IVL, &recover, sizeof(recover)); //设置socket的能从多播组缺席的最长时间 单位为ms,这段时间发送的数据会被保存在内存里
	int rc=zmq_connect(subscriber, "epgm://239.192.1.23:5555");   //sub端用connect连接到多播组
	assert(rc == 0);
	uint64_t index = 0;
	DAMarketdataHead* Test_head = new DAMarketdataHead;
	Test_head->timestamp = 0;
	uint64_t dt = 0;
	std::ofstream ofile;
	ofile.open("log.txt");
	char test[512];
	while (index<1000000) {
		index++;
		uint64_t timestamp = xdk_nanosecond_timestamp();
		zmq_recv(subscriber, Test_head, sizeof(DAMarketdataHead), 0);
		//memcpy(Test_head, test, sizeof(DAMarketdataHead));      //将接收到的数据转化成结构体
		if (timestamp > Test_head->timestamp) {                 //用来计算数据发送到数据接收的时间
			dt = (timestamp - Test_head->timestamp);
		}
		else {
			dt = Test_head->timestamp - timestamp;
		}
	if(dt>100000000)
		ofile <<Test_head->sequence<<','<< dt << std::endl;       //打印delay过长的数据
	}
	//  We never get here, but clean up anyhow
	zmq_close(subscriber);
	zmq_ctx_destroy(context);
	
}


int main() {

	auto thread1 = std::async(std::launch::async, EPGMSubscriberThread);
	thread1.wait();
}

