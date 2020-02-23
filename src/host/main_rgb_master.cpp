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

#define MAXBUF	10 * 1024


int mainApp(int argc, char **argv);
int main(int argc, char **argv)
{
	network_init();
	int ret = mainApp(argc, argv);
	network_cleanup();
}

#define OPMODE_SLAVE_LISTEN 0
#define OPMODE_FADE 1
#define OPMODE_ON 2
#define OPMODE_OFF 3
#define PKT_TYPE_SET_RGB 0
#define PKT_TYPE_SET_OPMODE 1
#define PKT_TYPE_REFRESH 2
#define MAX_LEDS 256

struct heartbeat_message {
  uint32_t client_id;
  uint32_t chk;
};
struct led_command {
  uint8_t type;
  uint8_t opmode;
  uint8_t pos;
  uint8_t len;
};
int mainApp(int argc, char **argv)
{

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
	char buf[MAXBUF];
	int ledRGBArray[MAX_LEDS];
	n = 0;
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
		const uint8_t lampId = pktHeartbeat.client_id;
		const uint8_t numLeds = pktHeartbeat.chk > MAX_LEDS ? MAX_LEDS: pktHeartbeat.chk;
//		printf("sendto %d\n", (int64_t)sockfd);
		led_command ledCommand = { PKT_TYPE_SET_OPMODE, OPMODE_SLAVE_LISTEN, 0, 0 };
		memcpy(buf, (void*) &ledCommand, sizeof(led_command));
		if (sendto(sockfd, buf, sizeof(led_command), 0, cliaddr, addrLen) < 0) {
			if (errno == ENOBUFS)
				continue;
			printf("server error: errno %d\n", errno);
			break;
		}
		ledCommand = { PKT_TYPE_SET_RGB, OPMODE_SLAVE_LISTEN, 0, numLeds };
		memcpy(buf, (void*) &ledCommand, sizeof(led_command));
		for (int i = 0; i < numLeds; i++) {
			uint32_t rgbVal = 0x996633;
			rgbVal &= 0x0;
			rgbVal |= rand()&0xFF;
			rgbVal <<= 8;
//			rgbVal |= rand()&0xFF;
			rgbVal <<= 8;
//			rgbVal |= rand()&0xFF;
			ledRGBArray[i] = rgbVal;
		}
		memcpy(buf+sizeof(led_command), (void*) ledRGBArray, sizeof(uint32_t)*numLeds);
		if (sendto(sockfd, buf, sizeof(led_command)+sizeof(uint32_t)*numLeds, 0, cliaddr, addrLen) < 0) {
			if (errno == ENOBUFS)
				continue;
			printf("server error: errno %d\n", errno);
			break;
		}
		ledCommand = { PKT_TYPE_REFRESH, brightness, 0, 255 };
		memcpy(buf, (void*) &ledCommand, sizeof(led_command));
		if (sendto(sockfd, buf, sizeof(led_command), 0, cliaddr, addrLen) < 0) {
			if (errno == ENOBUFS)
				continue;
			printf("server error: errno %d\n", errno);
			break;
		}


	}
	/* can't get here, but just in case: close sockets
	 */
	close(sockfd);
	return (0);

}
