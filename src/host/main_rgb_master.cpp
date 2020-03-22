/**
 * Copyright (c) 2018 Michael Hept
 */
#include <memory>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include "basectrl.h"
#include "color_util.h"
#include "platform.h"
#include "threads.h"
#include "assert_dbg.h"

#ifdef _WIN32
//  #define _WIN32_WINNT 0x501
//  #ifndef _CRT_SECURE_NO_WARNINGS
//    #define _CRT_SECURE_NO_WARNINGS
//  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
//  #include <windows.h>
typedef SOCKET sock_type_t;
#else
  //#define _POSIX_C_SOURCE 200809L
  #ifdef __APPLE__
    #define _DARWIN_UNLIMITED_SELECT
  #endif
  #include <signal.h>
  #include <unistd.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
typedef int sock_type_t;
#endif
//#include "network.h"
//#include "exceptions.h"

#ifdef _WIN32
  #define close(a) closesocket(a)
  #define getsockopt(a,b,c,d,e) getsockopt((a),(b),(c),(char*)(d),(e))
  #define setsockopt(a,b,c,d,e) setsockopt((a),(b),(c),(char*)(d),(e))
  #define select(a,b,c,d,e) select((int)(a),(b),(c),(d),(e))
  #define bind(a,b,c) bind((a),(b),(int)(c))
  #define connect(a,b,c) connect((a),(b),(int)(c))

  #undef  errno
  #define errno WSAGetLastError()

  #undef  EWOULDBLOCK
  #define EWOULDBLOCK WSAEWOULDBLOCK
	const char *inet_addrtocstr(int af, const void *src, char *dst, socklen_t size);
	int getImplErrno();
#else
	int getImplErrno();
	const char *inet_addrtocstr(int af, const void *src, char *dst, socklen_t size);
#endif
#ifndef INVALID_SOCKET
  #define INVALID_SOCKET -1
#endif

	void deleteApp() {

	}

	std::shared_ptr<AppCtrl> makeApp() {
		return nullptr;
	}
	void makeAppCompanions(std::vector<std::shared_ptr<AppCtrl>>& out_Companions) {
	}
void network_init(void);
void network_cleanup(void);

/*
 * udpserver.c
 *
 * This is an example TCP/IP UDP socket server.
 * It will read packets sent to 'portno' and write them back
 * to the sender.  It has no reliability functions.
 * It will loop forever and must be killed in order to make
 * it terminate.
 *
 * Protocol independant( working for both IPV4 and IPV6)
 * syntax:
 *      % udpserver portno &
 *
 * Start the server first and then start the udpclient.c app to
 * send packets to it.  Use the debug switch with udpclient.c to
 * see if any of the packets disappear.  They won't disappear on
 * localhost, but they might go away if they are crossing a busy
 * router.
// */
#include <stdio.h>

#define MAXBUF	32 * 1024


int mainApp(int argc, char **argv);
int main(int argc, char **argv)
{
	network_init();
	int ret = mainApp(argc, argv);
	network_cleanup();
}


#define OPMODE_DISPLAY 0
#define OPMODE_FADE 1
#define OPMODE_ON 2
#define OPMODE_OFF 3
#define PKT_TYPE_SET_CFG 1
#define PKT_TYPE_SET_RGB 2
#define PKT_TYPE_REFRESH 3

#define CFG_ID_COUNT 7

#define NUM_STRIPES 5U
#define NUM_LEDS 30U
#define NUM_TOTAL_LEDS (NUM_STRIPES*NUM_LEDS)
#define BRIGHTNESS          35 //255 is max, MAKE SURE YOUR PSU CAN PROVIDE THE AMPS!
#define LAMP_ID 2


#define FRAME_SIZE_BYTES (NUM_TOTAL_LEDS*sizeof(int32_t))
#define FRAME_ARRAY_LEN 64U


// MASTER SIDE PARAMETERS
#define FRAME_DURATION 24
#define FRAME_BUF_LEN 2

const char *ssid = "ddwrt"; // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "wifipassword";  // The password of the Wi-Fi network
;

struct heartbeat_message {
  uint32_t packetsReceived;
  uint8_t lampId;
  uint8_t opMode;
};

struct packet_hdr_t {
  uint16_t packetType;
  uint16_t len;
};

struct frame_hdr_t {
  uint16_t frameId;
  uint16_t numFrames;
  uint16_t frameSizeBytes;
};

enum cfg_type {
  CFG_ID_LEDPIN = 0,
  CFG_ID_BRIGHTNESS,
  CFG_ID_LAMPID,
  CFG_ID_NUMLEDCONNECTED,
  CFG_ID_HEARTBEATINTERVAL,
  CFG_ID_FRAME_DURATION,
  CFG_ID_OPMODE
};
#define CFG_ID_COUNT (((int)CFG_ID_OPMODE)+1)

struct lamp_config_t {
  uint32_t frameDuration_millis;
  uint32_t heartBeatInterval_millis;
  uint32_t lampId;
  uint32_t numLEDConnected;
  uint8_t ledPin;
  uint8_t brightness;
  uint8_t opMode;
};
struct lamp_context_t {
	uint64_t frameId = 0;
	uint64_t frameStep = 0;
	uint64_t packetId = 0;
};

struct lamp_state_t {
  bool opModeChanged;
  uint16_t writePosArrayIdx;
  uint16_t readPosArrayIdx;
  unsigned long lastFrameTime;
  unsigned long lastReceived;
  unsigned long lastHeartBeatSent;
};

static char buf[MAXBUF];
int mainApp(int argc, char **argv)
{

	printf("sizeof(struct packet_hdr_t) %d\n", sizeof(struct packet_hdr_t));
	printf("sizeof(struct heartbeat_message) %d\n", sizeof(struct heartbeat_message));
	  printf("sizeof(struct frame_hdr_t) %d\n", sizeof(struct frame_hdr_t));
	  printf("sizeof(struct lamp_config_t) %d\n", sizeof(struct lamp_config_t));
	  printf("sizeof(struct uint32_t) %d\n", sizeof(uint32_t));
	/* Check for arguments - hostname and portnumber required */
	if (argc<3) {
		printf("Too few arguments\n");
		printf("Usage: %s <hostname> <port>\n\n", argv[0]);
		return 1;
	}
	const uint8_t brightness = argc > 3 ? (uint8_t)atoi(argv[3]) : 55;
	struct addrinfo hints;
	/*initilize addrinfo structure*/
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int n;
	struct addrinfo *res;
	if ((n = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0)
	{
		printf("udpserver error for %s:%s - %s", argv[1], argv[2], gai_strerror(n));
		return 1;
	}
	sock_type_t sockfd;
	auto resIt = res;
	bool connected = false;
	do {/* each of the returned IP address is tried*/
		sockfd = socket(resIt->ai_family, SOCK_DGRAM, IPPROTO_UDP);
		if (sockfd >= 0) {
			if (bind(sockfd, resIt->ai_addr, resIt->ai_addrlen) == 0) {
				connected = true;
				break; /*success*/

			}
		}
		close(sockfd);
	} while ((resIt = resIt->ai_next) != NULL);

	if (!resIt) {
		printf("udpserver error for %s:%s - %s", argv[1], argv[2], gai_strerror(n));
		return 1;
	}
	socklen_t addrLen = resIt->ai_addrlen;
	freeaddrinfo(res);



	struct sockaddr *cliaddr = (struct sockaddr*) malloc(addrLen);
	std::map<int32_t, lamp_config_t> configs;
	std::map<int32_t, lamp_context_t> ctxts;

	n = 0;
	int masterFrameStep = 0;
	for (;;) { /* do forever */
		int rc;

		if ((rc = recvfrom(sockfd, buf, MAXBUF, 0, cliaddr, &addrLen)) < 0) {
			printf("server error: errno %d\n", errno);
			perror("reading datagram");
			break;
		}
		if (n < 10) {
			printf("recvfrom %d len %d\n", (int64_t)rc, addrLen);
		}
		n++;


		heartbeat_message pktHeartbeat;
		memcpy(&pktHeartbeat, buf, sizeof(heartbeat_message));
		const int16_t lampId = pktHeartbeat.lampId;
		printf("LAMP %d: Packets received %d\n", lampId, pktHeartbeat.packetsReceived);
		if (0==ctxts.count(lampId)) {
			ctxts[lampId] = lamp_context_t{};

		}
		if (0==configs.count(lampId) || pktHeartbeat.opMode != OPMODE_DISPLAY) {
			{
				packet_hdr_t hdr;
				hdr.len = sizeof(uint32_t)+sizeof(lamp_config_t);
				hdr.packetType = PKT_TYPE_SET_CFG;

				lamp_config_t cfg;
				cfg.opMode = OPMODE_DISPLAY;
				cfg.brightness = 70;
				cfg.heartBeatInterval_millis = 5000;
				cfg.frameDuration_millis = FRAME_DURATION;
				uint32_t mask = (1<<(int)CFG_ID_OPMODE)|(1<<(int)CFG_ID_BRIGHTNESS)|(1<<(int)CFG_ID_HEARTBEATINTERVAL)|(1<<(int)CFG_ID_FRAME_DURATION);
				configs[lampId] = cfg;
				char* bufPos = buf;
				memcpy(bufPos, (void*) &hdr, sizeof(packet_hdr_t));
				bufPos+=sizeof(packet_hdr_t);
				memcpy(bufPos, (void*) &mask, sizeof(uint32_t));
				bufPos+=sizeof(uint32_t);
				memcpy(bufPos, (void*) &cfg, sizeof(lamp_config_t));
				bufPos+=sizeof(lamp_config_t);
				printf("sendto %d\n", bufPos-buf);
				if (sendto(sockfd, buf, bufPos-buf, 0, cliaddr, addrLen) < 0) {
					int curErrNo = errno;

					printf("server error: errno %d\n", curErrNo);
					if (curErrNo == ENOBUFS)
						continue;
					break;
				}
			}
			printf("send frames to lamp %d\n", lampId);
			uint64_t tmStart = getTimeMillis();
			uint64_t tmStartHb = getTimeMillis();
			for (;;)
			{

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				uint64_t tmNow = getTimeMillis();
				uint64_t millisd = tmNow-tmStart;
				uint64_t millisHb = tmNow-tmStartHb;

				if (millisHb >= configs[lampId].heartBeatInterval_millis) {
					tmStartHb = getTimeMillis();

					if ((rc = recvfrom(sockfd, buf, MAXBUF, 0, cliaddr, &addrLen)) < 0) {
						printf("server error: errno %d\n", errno);
						perror("reading datagram");
						break;
					}
					if (n < 10) {
						printf("recvfrom %d len %d\n", (int64_t)rc, addrLen);
					}
					n++;


					heartbeat_message pktHeartbeat;
					memcpy(&pktHeartbeat, buf, sizeof(heartbeat_message));
					const int16_t lampId = pktHeartbeat.lampId;
					printf("LAMP %d: Packets received %d\n", lampId, pktHeartbeat.packetsReceived);
					if (0==ctxts.count(lampId)) {
						ctxts[lampId] = lamp_context_t{};

					}
				}
				if (millisd >= FRAME_DURATION*FRAME_BUF_LEN) {
					printf("send frames...\n");
					tmStart = getTimeMillis();


					lamp_context_t& ctxt = ctxts[lampId];
					ctxt.frameStep = masterFrameStep;
					packet_hdr_t hdr;
					hdr.len = sizeof(lamp_config_t);
					hdr.packetType = PKT_TYPE_SET_RGB;
					frame_hdr_t frameHdr;
					frameHdr.frameId = ctxt.frameId;
					frameHdr.numFrames = FRAME_BUF_LEN;
					frameHdr.frameSizeBytes = FRAME_SIZE_BYTES;
					dbgassert (sizeof(int32_t)*NUM_TOTAL_LEDS == FRAME_SIZE_BYTES);
					char* bufPos = buf;
					memcpy(bufPos, (void*) &hdr, sizeof(packet_hdr_t));
					bufPos+=sizeof(packet_hdr_t);
					memcpy(bufPos, (void*) &frameHdr, sizeof(frame_hdr_t));
					bufPos+=sizeof(frame_hdr_t);
					auto arr = new int[NUM_TOTAL_LEDS];
					for (int frameNr = 0; frameNr < FRAME_BUF_LEN; frameNr++) {
						ctxt.frameId++;
						ctxt.frameStep++;
						float ff = fmodf(ctxt.frameStep/20.0f, 1.0f);
						float ff2 = fmodf(ctxt.frameStep/7.0f, 1.0f);
						ff2 = 1.0f-abs(ff2-0.5f);
						ff = 1.0f-abs(ff-0.5f);
						ff *= ff;
						for (int i = 0; i < NUM_TOTAL_LEDS; i++) {
							int x = i%NUM_LEDS;
							int y = i/NUM_LEDS;
							if (lampId < 2 && ctxts.size() > 1) {
								y += NUM_STRIPES;
							}
							float fx = x/(float)(NUM_LEDS-1);

							float fy = y/(float)((NUM_STRIPES*2)-1);
		//					if (ctxts.size() > 1) {
		//						fy = fy*0.5f;
		//						if (lampId < 2) {
		//							fy += 0.5f;
		//						}
		//					}
							float cntx = (1.0f-abs(fx-0.5f));
							float cnty = (1.0f-abs(fy-0.5f));
							float b = pow(cntx, 1.0f+7.0f*ff)*pow(cnty, 1.0f+7.0f*ff);
							float g = pow(cntx, 1.8f+9.0f*ff)*pow(cnty, 1.6f+3.0f*ff2);
							float r = pow(cntx, 1.2f+2.0f*ff2)*pow(cnty, 1.0f+7.0f*ff);
							g = 0;
							r = 0;
							float sc = 0.3f;
							r*=sc;
							g*=sc;
							b*=sc;
							float fColHue = (cnty*cntx) + ctxt.frameStep*0.01f;
							NVGcolor col = HSLtoRGB(fmodf(fColHue, 1.0f), 1.0f, 1.0f);
							r = col.r;
							g = col.g;
							b = col.b;
							int intRGBA = math::max<int32_t>(0, math::min<int32_t>(255, math::round(b*255.0f)));
							intRGBA |= (math::max<int32_t>(0, math::min<int32_t>(255, math::round(g*255.0f)))&0xFF)<<8;
							intRGBA |= (math::max<int32_t>(0, math::min<int32_t>(255, math::round(r*255.0f)))&0xFF)<<16;


							arr[i] = intRGBA;
//							if ((ctxt.frameStep)%NUM_TOTAL_LEDS == i+(lampId-1)*NUM_LEDS) {
//								arr[i] = 0x2288dd;
//							}
//							if (i < lampId && ((ctxt.frameStep/6)%6) < 6/3) {
//								arr[i] = 0x770077;
//							}
						}
						memcpy(bufPos, (void*) &arr[0], sizeof(int32_t)*NUM_TOTAL_LEDS);
						bufPos+=sizeof(int32_t)*NUM_TOTAL_LEDS;
						if (lampId <= 1) {
							masterFrameStep = ctxt.frameStep;
						}
					}
					delete[] arr;
					ctxt.packetId++;
					printf("send lampId %d, packetId %d, frameId %d size %d\n", lampId, ctxt.packetId, frameHdr.frameId, ((size_t)bufPos-(size_t)buf));
					for (int i = 0; i < 1; i++) {
						if (sendto(sockfd, buf, bufPos-buf, 0, cliaddr, addrLen) < 0) {
							int curErrNo = errno;

							printf("server error: errno %d\n", curErrNo);
							if (curErrNo == ENOBUFS)
								continue;
							break;
						}
					}
				}
			}
		}
//		const int16_t numLeds = pktHeartbeat.chk > NUM_LEDS ? NUM_LEDS: pktHeartbeat.chk;
////		printf("sendto %d\n", (int64_t)sockfd);
//		led_command ledCommand = { PKT_TYPE_SET_OPMODE, OPMODE_SLAVE_LISTEN, 0, 0 };
//		memcpy(buf, (void*) &ledCommand, sizeof(led_command));
//		if (sendto(sockfd, buf, sizeof(led_command), 0, cliaddr, addrLen) < 0) {
//			if (errno == ENOBUFS)
//				continue;
//			printf("server error: errno %d\n", errno);
//			break;
//		}
//		ledCommand = { PKT_TYPE_SET_RGB, OPMODE_SLAVE_LISTEN, 0, (uint8_t)numLeds };
//		memcpy(buf, (void*) &ledCommand, sizeof(led_command));
//		int n = rand()%(numLeds/2);
//		for (int i = 0; i < numLeds; i++) {
//			uint32_t rgbVal = 0x996633;
//			rgbVal &= 0x0;
//			rgbVal |= math::min(255, math::max(0, (math::max(0, i-n)*455/(numLeds-1))))&0xFF;
//			rgbVal <<= 8;
//			rgbVal |= math::min(255, math::max(0, (math::max(0, i-n)*355/(numLeds-1))))&0xFF;
//			rgbVal <<= 8;
//			rgbVal |= math::min(255, math::max(0, (math::max(0, i-n)*255/(numLeds-1))))&0xFF;
//			ledRGBArray[i] = rgbVal;
//		}
//		memcpy(buf+sizeof(led_command), (void*) ledRGBArray, sizeof(uint32_t)*numLeds);
//		if (sendto(sockfd, buf, sizeof(led_command)+sizeof(uint32_t)*numLeds, 0, cliaddr, addrLen) < 0) {
//			if (errno == ENOBUFS)
//				continue;
//			printf("server error: errno %d\n", errno);
//			break;
//		}
//		ledCommand = { PKT_TYPE_REFRESH, brightness, 0, 255 };
//		memcpy(buf, (void*) &ledCommand, sizeof(led_command));
//		if (sendto(sockfd, buf, sizeof(led_command), 0, cliaddr, addrLen) < 0) {
//			if (errno == ENOBUFS)
//				continue;
//			printf("server error: errno %d\n", errno);
//			break;
//		}
//

	}
	/* can't get here, but just in case: close sockets
	 */
	close(sockfd);
	return (0);

}
