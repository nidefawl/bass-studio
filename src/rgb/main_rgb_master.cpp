/**
 * Copyright (c) 2020 Michael Hept
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
#include "net/network.h"
#include "net/packet.h"
#include "net/stream/audiostream.h"
#include "threads/workerthread.h"
#include "logging.h"
#include "udp_sync_server.h"
#include "rgbmaster.h"

#ifdef _WIN32
#include <io.h>
#endif
#ifdef _WIN32
#include <windows.h>
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

void openGlobalLog(const String& logFileName); // Forward declare from util/debug.cpp
void closeGlobalLog(); // Forward declare from util/debug.cpp

int mainApp(int argc, char **argv);
int main(int argc, char **argv)
{
	setCurrentThreadName("mainthread");
	network_init();
#if !defined(NDEBUG) && defined(_WIN32)
    _dup2( 1, 2 ); //workaround: redirect stderr to stdout so stderr is visible when using gdb on eclipse (bug)
#endif
#ifdef USE_WIN32_EXC_HOOKS
	setExceptionHandler();
#endif
	//if (!runConsoleMode) {
	allocConsole();
	//}
	openGlobalLog("rgbmaster.log");
	log_out("Start\n", 0);
	char* pPath;
	pPath = getenv("PATH");
	if (pPath != NULL)
		log_printf ("getenv PATH: %s\n",pPath);
	int ret = mainApp(argc, argv);
	closeGlobalLog();
	network_cleanup();
}

#include "C:/dev/esp8266/projects/UPD_RGB_DISPLAY/app/rgb_network_types.h"



// MASTER SIDE PARAMETERS
#define FRAME_DURATION 12
#define FB_LEN 16
#define HB_INTERVAL 2000
#define FRAME_BUF_LEN 6
#define RESYNC_FRAMES (FRAME_ARRAY_LEN*8)



class rgbprotocol_net_handler_client : public inetwork_handler {
public:
	std::shared_ptr<network_conn_t> conn;
	std::vector<uint8_t> buf;
	bool connected = false;
	rgbprotocol_net_handler_client() = delete;
	rgbprotocol_net_handler_client(std::shared_ptr<network_conn_t> _conn) : conn(_conn) {

	}
	void onError(int errorType, String msg) override {
		log_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		buf.insert(buf.end(), reinterpret_cast<uint8_t*>(data), reinterpret_cast<uint8_t*>(data)+size);
		return true;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
		conn->setSocketOpt(IPPROTO_TCP, TCP_NODELAY, 1);
#endif
		conn->parent->setSelectTimeout(0.0001);
		log_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		log_printf("disconnected\n", 0);
		connected = false;
	}

	void writeBuffer(void* buf, size_t size) {
		conn->write(buf, size);
	}
};
struct rgbprotocol_server_client_conn_t {
	std::shared_ptr<network_conn_t> conn;
	std::shared_ptr<rgbprotocol_net_handler_client> handler;
	bool init = false;
	int packetsReceived = 0;
	int lampId = -1;
};
class rgbprotocol_net_handler_server : public inetwork_handler {
public:
	std::vector<std::shared_ptr<rgbprotocol_server_client_conn_t>> conns;
	bool connected = false;
	rgbprotocol_net_handler_server() {

	}
	void writeBufferToAll(const String& strBuf) {
		writeBufferToAll((void*)strBuf.c_str(), strBuf.length());
	}
	void writeBufferToAll(void* buf, size_t size) {
		for (auto& pc : conns) {
			pc->handler->writeBuffer(buf, size);
		}
	}
	void onError(int errorType, String msg) override {
		log_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		dbgassert(0); // not called
		return 0;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
		conn->setSocketOpt(IPPROTO_TCP, TCP_NODELAY, 1);
#endif
		conn->parent->setSelectTimeout(0.0001);
		log_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		log_printf("disconnected\n", 0);
		connected = false;
	}
	virtual bool onAccept(std::shared_ptr<network_conn_t> clientConn) {
		log_printf("onAccept %s\n", StringAsCStr(clientConn->address));
		auto handler = std::make_shared<rgbprotocol_net_handler_client>(clientConn);
		clientConn->handler = handler.get();
		conns.push_back(std::shared_ptr<rgbprotocol_server_client_conn_t>(new rgbprotocol_server_client_conn_t{clientConn, handler}));
		return true;
	}
};
struct frame_rgb_buffer_t {
	uint32_t* pData;
	size_t lenData;
	uint32_t w;
	uint32_t h;
};
struct frame_render_ctxt_t {
	uint32_t frameStep;
	int16_t lampId;
};
static void writeToFrameBuffer(frame_rgb_buffer_t& buffer, int32_t x, int32_t y, vec4 col) {

	if (x < 0 || x >= buffer.w || y < 0 || y >= buffer.h)
		return;
	uint32_t idx = y*buffer.w+x;
	if (idx < buffer.lenData) {
		uint32_t intRGBA = math::max<int32_t>(0, math::min<int32_t>(255, math::round(col.b*255.0f)));
		intRGBA |= (math::max<int32_t>(0, math::min<int32_t>(255, math::round(col.g*255.0f)))&0xFF)<<8;
		intRGBA |= (math::max<int32_t>(0, math::min<int32_t>(255, math::round(col.r*255.0f)))&0xFF)<<16;

		buffer.pData[idx] = intRGBA;
	}

}
struct RGBNetworkController {
	struct lamp_context_t {
		uint32_t frameId = 0;
		uint32_t frameStep = 0;
		uint64_t packetId = 0;
	};

	int64_t lastResyncCycle = -1;
	int64_t tmLastSync=0;
	int64_t tmBegin=0;
	std::map<int32_t, lamp_config_t> configs;
	std::map<int32_t, lamp_context_t> ctxts;
	int n = 0;
//	int masterFrameStep = 0;
	packet_hdr_t recvHeader;
	uint8_t writeBuf[RGB_PROTOCOL_WRITE_BUF_SIZE];
	uint32_t arr[NUM_TOTAL_LEDS];
	uint32_t curProgram = 0;
	void runProgram0(frame_render_ctxt_t& renderCtxt, frame_rgb_buffer_t& buffer) {
		if (0 == renderCtxt.frameStep) {
			memset(arr, 0, sizeof(uint32_t)*NUM_TOTAL_LEDS);
		}
		const uint32_t frameStep = renderCtxt.frameStep;
		const int16_t lampId = renderCtxt.lampId;
		for (int xOut = 0; xOut < buffer.w; xOut++) {
			for (int yOut = 0; yOut < buffer.h; yOut++) {

				int x = xOut;
				int y = yOut;
				if (lampId >= 2) {
					y += buffer.h;
				}
				float fx = x/(float)(NUM_LEDS-1);
				float fy = y/(float)((NUM_STRIPES*2)-1);
				float ff = fmodf(frameStep/120.0f, 1.0f);
				float ff2 = fmodf(frameStep/7.0f, 1.0f);
		//						ff = ff2 = fmodf(ctxt.frameStep/166.0f, 1.0f);
				ff2 = 1.0f-abs(ff2-0.5f)*2.0f;
				ff = 1.0f-abs(ff-0.5f)*2.0f;
				ff *= ff;

				float cntx = (1.0f-abs(fx-0.5f)*2.0f) + 0.0f;
				float cnty = (1.0f-abs(fy-0.5f)*2.0f) + 0.12f;
				float g = pow(cntx, 1.8f+9.0f*ff)*pow(cnty, 1.6f+3.0f*ff2)*1.1f;
				float r = pow(cntx, 1.2f+2.0f*ff2)*pow(cnty, 1.0f+7.0f*ff)*1.2f;
		//							float b = pow(cntx, 1.0f+7.0f*ff)*pow(cnty, 1.0f+7.0f*ff);
		//							float g = pow(cntx, 1.8f+9.0f*ff)*pow(cnty, 1.6f+3.0f*ff2);
		//							float r = pow(cntx, 1.2f+2.0f*ff2)*pow(cnty, 1.0f+7.0f*ff);
		//							float r, g;
				float b = 0;
				g = 0;
				r = 0;
		//							float sc = ((ctxt.frameStep%1000)/1000.0f)*244.0f/256.0f;
		//							if (ff2*ff2>0.3*ff) {
		//								sc+=0.4f;
		//							}
				float fColHue = (cnty*cntx) + (frameStep+y)*0.01f;
				vec4 hsl = rgbToHSL(math::clamp(r, 0.0f, 1.0f), math::clamp(g, 0.0f, 1.0f), math::clamp(b, 0.0f, 1.0f));
				NVGcolor col = HSLtoRGB(0.05+fmodf(fColHue, 1.0f)*0.9, 0.98f, 0.4f);
				r = math::max(0.0f, cntx-0.9f)*1.0f*cnty*col.r*(0.5+0.5*sin(ff*3.1495*2.0+M_PI*0.0f/3.0f));
				g = math::max(0.0f, cntx-0.9f)*1.0f*cnty*col.g*(0.5+0.5*sin(ff*3.1495*2.0+M_PI*2.0f/3.0f));
				b = math::max(0.0f, cntx-0.9f)*1.0f*cnty*col.b*(0.5+0.5*sin(ff*3.1495*2.0+M_PI*4.0f/3.0f));
				r=col.r;
				g=col.g;
				b=col.b;
				if ((x&1)^(y&1)) {
		//								float ffff = fmodf(fx+(ctxt.frameStep+y)*0.05f, 1.0f);
		//								NVGcolor col = HSLtoRGB(0.05+(1.0-0.05)*fx*fy*ff, (0.98), 0.39f);
		//								b += pow(cntx, 1.6f+1.0f*ff)*pow(cnty, 1.6f+6.0f*ff2)*1.1f;
					float sc=0.2+(6.4f)*(0.5+0.5*sin(3.1495f*2*((frameStep)*0.00754f)));
					r*=sc;
					g*=sc;
					b*=sc;

				} else {
					if ((frameStep/66)%8<4) {
						vec4 hsl = rgbToHSL(r, g, b);
						col = HSLtoRGB(fmodf(hsl.x+0.5f, 1.0f), hsl.y, hsl.z);
						r=col.r;
						g=col.g;
						b=col.b;
					}
		//								g += pow(cntx, 1.6f+2.0f*ff)*pow(cnty, 1.6f+4.0f*ff2)*2.1f;
				}
				fColHue = hsl.r+fColHue;
				fColHue = fmodf(fColHue, 1.0f);
		//							NVGcolor col = HSLtoRGB(fColHue, hsl.g, hsl.b);
				float ffff = fmodf(fx+(frameStep+y)*0.05f, 1.0f);
		//							NVGcolor col = HSLtoRGB(0.05+(1.0-0.05)*fx*fy*ff, (0.98), 0.39f);
		//							NVGcolor col = HSLtoRGB(0.05+(1.0-0.05)*(rand.rng_bits(12)/4095.0f), (0.98), 0.0f+0.5f*(rand.rng_bits(12)/4095.0f));

				float sc2 = 0.2f;
				r*=sc2;
				g*=sc2;
				b*=sc2;
		//							g=col.g*sc;
		//							b=col.b*sc;
		//							if (x*y<ctxt.frameStep%NUM_TOTAL_LEDS*2) {
		//								r+=0.3f;
		//							}
		//							if (x+y*NUM_LEDS==ctxt.frameStep%NUM_TOTAL_LEDS*2) {
		//								b+=0.3f;
		//							}
				writeToFrameBuffer(buffer, xOut, yOut, vec4{r,g, b, 1.0f});
			}

		}
	}
	void runProgram2(frame_render_ctxt_t& renderCtxt, frame_rgb_buffer_t& buffer) {
		const uint32_t frameStep = renderCtxt.frameStep;
		const int16_t lampId = renderCtxt.lampId;
		if (0 == renderCtxt.frameStep) {
		}
		seq_rand rand;
		rand.rng_seed(frameStep>>1);
		memset(arr, 0, sizeof(uint32_t)*NUM_TOTAL_LEDS);
		for (int xOut = 0; xOut < buffer.w; xOut++) {
			for (int yOut = 0; yOut < buffer.h; yOut++) {
				if (rand.rng_bits(8)) {
					writeToFrameBuffer(buffer, xOut, yOut, vec4{0, 0, (rand.rng_rand()&0xFFF)/(float)0xFFF, 1});
				} else {
					writeToFrameBuffer(buffer, xOut, yOut, vec4{1, 1, 1, 1}*0.5f);
				}

			}
		}
	}
	void runProgram3(frame_render_ctxt_t& renderCtxt, frame_rgb_buffer_t& buffer) {
		const uint32_t frameStep = renderCtxt.frameStep;
		const int16_t lampId = renderCtxt.lampId;
		memset(arr, 0, sizeof(uint32_t)*NUM_TOTAL_LEDS);
		for (int xOut = 0; xOut < buffer.w; xOut++) {
			for (int yOut = 0; yOut < buffer.h; yOut++) {

				int x = xOut;
				int y = yOut;
				if (lampId >= 2) {
					y += buffer.h;
				}
				float fx = x/(float)(NUM_LEDS-1);
				float fy = y/(float)((NUM_STRIPES*2)-1);
				float r, g, b, a;
				r = g = b = a = 1.0f;
				r = fmodf(frameStep/120.0f, 1.0f);
				g = fy;
				b = fx;
				if (frameStep%NUM_LEDS == x) {
					r = g = b = 0;
				}
				if ((frameStep/3)%(NUM_STRIPES*2) == y) {
					r = g = b = 0;
				}
				writeToFrameBuffer(buffer, xOut, yOut, vec4{r,g,b,a});

			}
		}
	}
	void runProgram1(frame_render_ctxt_t& renderCtxt, frame_rgb_buffer_t& buffer) {
		const uint32_t frameStep = renderCtxt.frameStep;
		const int16_t lampId = renderCtxt.lampId;
		if (0 == renderCtxt.frameStep) {
			memset(arr, 0, sizeof(uint32_t)*NUM_TOTAL_LEDS);
		}

		for (int xOut = 0; xOut < buffer.w; xOut++) {
			for (int yOut = 0; yOut < buffer.h; yOut++) {

				writeToFrameBuffer(buffer, xOut, yOut, vec4{0, 0, 0, 1});
			}
		}

		int X = buffer.w;
		int Y = buffer.h*2;
	    int x,y,dx,dy;
	    x = y = dx =0;
	    dy = -1;
	    int t = std::max(X,Y);
	    int maxI = t*t;
	    int numWrites = 0;
	    int spiralLen = (renderCtxt.frameStep>>1)%(NUM_TOTAL_LEDS*2);
	    float fstep = 0;
	    for(int i =0; i < maxI && numWrites < spiralLen; i++){
	        if ((-X/2 <= x) && (x <= X/2) && (-Y/2 <= y) && (y < Y/2)){
	        	int32_t posX = (X/2)+x-1;
	        	int32_t posY = (Y/2)+y;
	        	if (lampId >= 2) {
	        		posY -= buffer.h;
	        	}
	        	float f = fmodf((numWrites) * (1.0f/(float)(NUM_TOTAL_LEDS*2.0f)), 1.0f);
//				NVGcolor col = HSLtoRGB(0.05f+f*0.9f, 0.98f, 0.5f);
				float fx = x/(float)(NUM_LEDS-1);
				float fy = y/(float)((NUM_STRIPES*2)-1);
				NVGcolor col = HSLtoRGB(0.05f+fx*0.9f, 0.01f+(1.0f-f)*0.98f, 1.0f);

				writeToFrameBuffer(buffer, posX, posY, vec4{col.r, col.g, col.b, 1});
				numWrites++;
	            // DO STUFF...
	        }
	        if( (x == y) || ((x < 0) && (x == -y)) || ((x > 0) && (x == 1-y))){
	            t = dx;
	            dx = -dy;
	            dy = t;
	        }
	        x += dx;
	        y += dy;
	    }

	}
	bool processPacket(rgbprotocol_server_client_conn_t* const conn, std::vector<uint8_t>& dataBuf) {
		n = 0;
//		if (n < 10) {
//			log_printf("recvfrom %d len %d\n", (int64_t)rc, addrLen);
//		}
		n++;
		//log_printf("dataBuf.size() %d\n", dataBuf.size());
		if (dataBuf.size() < sizeof(packet_hdr_t)) {
			return false;
		}
		uint8_t* const readBuf = dataBuf.data();
		uint8_t* readBufPos = readBuf;
		memcpy(&recvHeader, readBufPos, sizeof(packet_hdr_t));
		/*log_printf("hdr.packetType %d\n", recvHeader.packetType);
		log_printf("hdr.lampId %d\n", recvHeader.lampId);
		log_printf("hdr.len %d\n", recvHeader.len);
		log_printf("hdr.reserved %d\n", recvHeader.reserved);*/
		if (dataBuf.size() < sizeof(packet_hdr_t)+recvHeader.len) {
			return false;
		}
		readBufPos += sizeof(packet_hdr_t);
		if (conn->lampId < 0) {
			conn->lampId = recvHeader.lampId;
		}
		switch (recvHeader.packetType) {
		case PKT_TYPE_HEARTBEAT:
			{

				heartbeat_message pktHeartbeat;
				memcpy(&pktHeartbeat, readBufPos, sizeof(heartbeat_message));
				readBufPos += sizeof(heartbeat_message);
				const int16_t lampId = pktHeartbeat.lampId;
				if (conn->packetsReceived%100==0) {
					log_printf("LAMP %d Heartbeat: Packets received %d\n", lampId, pktHeartbeat.packetsReceived);
				}

				if (0==configs.count(lampId)||!conn->init)
				{
					if (0==configs.count(lampId) && lampId <= 1) {
						tmBegin = getTimeHPint64();
					}
					lastResyncCycle = -1;
//					masterFrameStep = 0;
					conn->init = true;
					packet_hdr_t hdr;
					hdr.len = sizeof(uint32_t)+sizeof(lamp_config_t);
					hdr.packetType = PKT_TYPE_SET_CFG;

					lamp_config_t cfg;
					cfg.opMode = OPMODE_DISPLAY;
					cfg.brightness = 64;
					cfg.heartBeatInterval_millis = HB_INTERVAL;
					cfg.frameDuration_millis = FRAME_DURATION;
					cfg.frameBufferUpdateLen = FB_LEN;
					cfg.showDebug = 0;
					uint32_t mask = (1<<(int)CFG_ID_OPMODE)
									|(1<<(int)CFG_ID_BRIGHTNESS)
									|(1<<(int)CFG_ID_SHOWDEBUG_ON_LED)
							|(1<<(int)CFG_ID_HEARTBEATINTERVAL)
							|(1<<(int)CFG_ID_FRAME_DURATION)|(1<<(int)CFG_ID_FRAMEBUFFER_UPDATE_LEN);
					configs[lampId] = cfg;
					uint8_t* bufPos = writeBuf;
					memcpy(bufPos, (void*) &hdr, sizeof(packet_hdr_t));
					bufPos+=sizeof(packet_hdr_t);
					memcpy(bufPos, (void*) &mask, sizeof(uint32_t));
					bufPos+=sizeof(uint32_t);
					memcpy(bufPos, (void*) &cfg, sizeof(lamp_config_t));
					bufPos+=sizeof(lamp_config_t);
					log_printf("send lamp %d lampId config packet with size %d\n", lampId, bufPos-writeBuf);
					conn->conn->write(writeBuf, bufPos-writeBuf);
					conn->conn->flush();
				}
			}
				break;
			case PKT_TYPE_REQUEST_FRAMES:
			{

				request_frames_message msgReqFrames;
				memcpy(&msgReqFrames, readBufPos, sizeof(request_frames_message));
				readBufPos += sizeof(request_frames_message);
				const int16_t lampId = msgReqFrames.lampId;
				if (msgReqFrames.numFrames == 0) {
					log_printf("lamp %d msgReqFrames.numFrames == %d\n", lampId, msgReqFrames.numFrames);
					break;
				}
//				log_printf("send %d frames to lamp %d\n", msgReqFrames.numFrames, lampId);
				uint16_t reqFrames = msgReqFrames.numFrames;
				uint16_t framesPerPacket = (1000U-sizeof(packet_hdr_t)-sizeof(frame_hdr_t))/(FRAME_SIZE_BYTES);

				lamp_context_t& ctxt = ctxts[lampId];
				ctxt.frameStep = msgReqFrames.frameId;
				ctxt.frameId = msgReqFrames.frameId;
				packet_hdr_t sendHeader;
				sendHeader.packetType = PKT_TYPE_SET_RGB;
				frame_hdr_t frameHdr;
				frameHdr.frameSizeBytes = FRAME_SIZE_BYTES;
				dbgassert(sizeof(int32_t)*NUM_TOTAL_LEDS == FRAME_SIZE_BYTES);
				int framesTransmitted = 0;
				for (int framesTransmitted = 0;framesTransmitted<reqFrames;) {
					uint16_t transmitFrames = math::min<uint16_t>(framesPerPacket, reqFrames-framesTransmitted);
					frameHdr.frameId = ctxt.frameId;
					frameHdr.numFrames = transmitFrames;
					sendHeader.len = sizeof(frame_hdr_t) + frameHdr.frameSizeBytes*transmitFrames;
					uint8_t *bufPos = writeBuf;
					memcpy(bufPos, (void*) &sendHeader, sizeof(sendHeader));
					bufPos += sizeof(packet_hdr_t);
					memcpy(bufPos, (void*) &frameHdr, sizeof(frameHdr));
					bufPos += sizeof(frame_hdr_t);
					seq_rand rand;
					frame_render_ctxt_t renderCtxt;
					frame_rgb_buffer_t buffer;
					buffer.w = NUM_LEDS;
					buffer.h = NUM_STRIPES;
					buffer.pData = arr;
					buffer.lenData = NUM_TOTAL_LEDS;
					for (int frameNr = 0; frameNr < transmitFrames; frameNr++) {
						rand.rng_seed(ctxt.frameStep);
						renderCtxt.lampId = msgReqFrames.lampId;
						renderCtxt.frameStep = ctxt.frameStep;
						switch (curProgram%4) {
						default:
						case 0:
							runProgram0(renderCtxt, buffer);
							break;
						case 1:
							runProgram1(renderCtxt, buffer);
							break;
						case 2:
							runProgram2(renderCtxt, buffer);
							break;
						case 3:
							runProgram3(renderCtxt, buffer);
							break;
						}

						memcpy(bufPos, (void*) &arr[0], sizeof(int32_t)*NUM_TOTAL_LEDS);
						bufPos+=sizeof(int32_t)*NUM_TOTAL_LEDS;
						dbgassert((size_t)(bufPos-writeBuf) < RGB_PROTOCOL_WRITE_BUF_SIZE);
						ctxt.frameId++;
						ctxt.frameStep++;
					}
					ctxt.packetId++;
//					log_printf("send lampId %d, packetId %d, frameId %d numFrames %d size %d\n", lampId, ctxt.packetId, frameHdr.frameId, frameHdr.numFrames, ((size_t)bufPos-(size_t)writeBuf));
					conn->conn->write(writeBuf, bufPos-writeBuf);
					conn->conn->flush();
//					std::this_thread::sleep_for(std::chrono::microseconds(100));
					framesTransmitted += transmitFrames;
				}
			}
				break;
			default:
				log_printf("Unhandled PacketType %d\n", recvHeader.packetType);
				break;
		}
		conn->packetsReceived++;
		//log_printf("buf erase [0-%d] %d\n", (readBufPos-readBuf), sizeof(packet_hdr_t)+recvHeader.len);
		//log_printf("buf size1 %d\n", dataBuf.size());
		dataBuf.erase(dataBuf.begin(), dataBuf.begin()+(sizeof(packet_hdr_t)+recvHeader.len));
		//log_printf("buf size2 %d\n", dataBuf.size());
		return true;
	}

};

RGBMasterController::RGBMasterController()
	: WorkerThread::ThreadTask(), controller(new RGBNetworkController{}), handler(new rgbprotocol_net_handler_server{}) {
}
RGBMasterController::~RGBMasterController() {
	delete controller;
	delete handler;
}

void RGBMasterController::run() {
	const size_t NUM_CHUNKS_FRAME_ARRAY = FRAME_ARRAY_LEN/FB_LEN;
	network_io netio(handler);
	std::shared_ptr<network_conn_t> listenSocket;
	if (netio.listenAt("192.168.0.228", 2123, protocol_type_i32::TCP, listenSocket)) {
		log_printf("bound to 0.0.0.0:2123\n", 0);
		netio.setSelectTimeout(0.0001);
		while (netio.hasOpenSockets() && !threadState.shouldQuit) {
			netio.update();

			threadState.isConnected = handler->conns.size() > 0;
			for (auto& pc : handler->conns) {
				std::vector<uint8_t>& dataBuf = pc->handler->buf;
				if (!dataBuf.size()) {
					continue;
				}
				while (controller->processPacket(pc.get(), dataBuf));
			}
			if (controller->tmBegin != 0) {
				int64_t millisSinceBegin = (getTimeHPint64()-controller->tmBegin)/1000U;
				//reset
				if (millisSinceBegin > 120*1000) {
					controller->tmBegin = getTimeHPint64();
					controller->lastResyncCycle = -1;
					millisSinceBegin = 0;
//						controller.curProgram++;
				}
//					millisSinceBegin += 10;
				int64_t resyncCycle = (millisSinceBegin / FRAME_DURATION)/RESYNC_FRAMES;
				if (resyncCycle != controller->lastResyncCycle) {
					controller->lastResyncCycle = resyncCycle;
					auto since = getTimeHPint64()-controller->tmLastSync;
					controller->tmLastSync=getTimeHPint64();

					sync_message pktSync;
					pktSync.frame = resyncCycle*RESYNC_FRAMES;
					packet_hdr_t hdr;
					hdr.len = sizeof(pktSync);
					hdr.packetType = PKT_TYPE_SYNC;
					uint8_t* bufPos = writeBuf;
					memcpy(bufPos, (void*) &hdr, sizeof(packet_hdr_t));
					bufPos += sizeof(packet_hdr_t);
					memcpy(bufPos, (void*) &pktSync, sizeof(pktSync));
					bufPos += sizeof(pktSync);

					for (auto& pc : handler->conns) {

						pc->conn->write(writeBuf, bufPos - writeBuf);
						pc->conn->flush();
					}
					log_printf("controller.resync resyncCycle %d\n", resyncCycle);
				}
			}
//				if (tmDurationLastSync/1000 >= 5000) {
//					tmLastSync = tmNow;
//					for (auto& pc : handler.conns) {
//						GlobalUDP_sendSyncTo(pc->conn->address, 7001);
//					}
//				}
//				long tmDurationLastSync = tmNow - tmLastSync;
		}
	}

	threadState.isRunning=false;
}
static bool quitServer = false;
extern bool quitUDPServer;
#ifdef _WIN32
static BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    switch(dwType) {
    case CTRL_C_EVENT:
		log_printf("CTRL_C\n", 0);
		quitServer = true;
    	quitUDPServer = true;
        break;
    }
    return TRUE;
}
#endif
int mainApp(int argc, char **argv)
{
	network_init();

	log_printf("sizeof(struct packet_hdr_t) %d\n", sizeof(struct packet_hdr_t));
	log_printf("sizeof(struct heartbeat_message) %d\n", sizeof(struct heartbeat_message));
	  log_printf("sizeof(struct frame_hdr_t) %d\n", sizeof(struct frame_hdr_t));
	  log_printf("sizeof(struct lamp_config_t) %d\n", sizeof(struct lamp_config_t));
	  log_printf("sizeof(struct uint32_t) %d\n", sizeof(uint32_t));

	/* Check for arguments - hostname and portnumber required */
	if (argc<3) {
		log_printf("Too few arguments\n", 0);
		log_printf("Usage: %s <hostname> <port>\n\n", argv[0]);
		return 1;
	}
	{

	    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)) {
	        fprintf(stderr, "Unable to install handler!\n");
	        return EXIT_FAILURE;
	    }
		log_printf("\n== START OF THREAD TEST ==\n", 0);
		static RGBMasterController rgbmaster;
		static WorkerThread thread;
		static WorkerThread thread2;
		WorkerThread* threads[] = { &thread, &thread2};
		std::shared_ptr<WorkerThread::ThreadTask> udpServerTask = createUDPServer();
		for (auto* t : threads) {
			t->startThread();
		}
		threads[0]->pushTask(&rgbmaster);
		threads[1]->pushTask(udpServerTask.get());
		while (!quitServer) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
		log_printf("state.shouldQuit = true\n", 0);
		rgbmaster.threadState.shouldQuit = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(210));
		for (auto* t : threads) {
			log_printf("thread.stopThread\n", 0);
			t->stopThread();
		}
		for (auto* t : threads) {
			log_printf("thread.joinThread\n", 0);
			t->joinThread();
		}
		thread.stopThread();
		thread.joinThread();
		log_printf("== END OF THREAD TEST ==\n\n", 0);
	}


	return (0);

}
