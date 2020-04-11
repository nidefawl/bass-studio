#pragma once
#define LED_PIN1 5
#define LED_PIN2 4
#define LED_PIN3 14
#define LED_PIN4 12
#define LED_PIN5 13
#define NUM_STRIPES 5U
#define NUM_LEDS 30U
#define NUM_TOTAL_LEDS (NUM_STRIPES*NUM_LEDS)
#define BRIGHTNESS          35 //255 is max, MAKE SURE YOUR PSU CAN PROVIDE THE AMPS!

#define OPMODE_DISPLAY 0
#define OPMODE_FADE 1
#define OPMODE_ON 2
#define OPMODE_OFF 3
#define PKT_TYPE_HEARTBEAT 0
#define PKT_TYPE_REQUEST_FRAMES 1
#define PKT_TYPE_SET_CFG 2
#define PKT_TYPE_SET_RGB 3
#define PKT_TYPE_REFRESH 4
#define PKT_TYPE_SYNC 5

#define FRAME_SIZE_BYTES (NUM_TOTAL_LEDS*sizeof(uint32_t))
#define FRAME_ARRAY_LEN 48U

#pragma pack(push, 2)
struct sync_message {
	uint32_t frame = 0;
};
struct packet_hdr_t {
	uint16_t packetType;
	uint16_t len;
	uint8_t lampId = 0;
	uint8_t reserved = 0;
};
struct heartbeat_message {
	uint32_t packetsReceived = 0;
	uint8_t lampId = 0;
	uint8_t opMode = 0;
	uint16_t d = 0;
};
struct request_frames_message {
	uint32_t frameId = 0;
	uint32_t numFrames = 0;
	uint16_t reqId = 0;
	uint8_t lampId = 0;
	uint8_t d = 0;
};
struct packet_heartbeat_t {
	packet_hdr_t hdr;
	heartbeat_message msgHeartbeat;
};
struct packet_request_frames_message_t {
	packet_hdr_t hdr;
	request_frames_message msgReqFrames;
};

struct frame_hdr_t {
	uint32_t frameId = 0;
	uint16_t numFrames = 0;
	uint16_t frameSizeBytes = 0;
};
#pragma pack(pop)

enum cfg_type {
	CFG_ID_LEDPIN = 0,
	CFG_ID_BRIGHTNESS,
	CFG_ID_LAMPID,
	CFG_ID_SHOWDEBUG_ON_LED,
	CFG_ID_NUMLEDCONNECTED,
	CFG_ID_HEARTBEATINTERVAL,
	CFG_ID_FRAMEBUFFER_UPDATE_LEN,
	CFG_ID_FRAME_DURATION,
	CFG_ID_OPMODE
};
#define CFG_ID_COUNT (((int)CFG_ID_OPMODE)+1)

struct lamp_config_t {
	uint32_t frameDuration_millis;
	uint32_t heartBeatInterval_millis;
	uint32_t frameBufferUpdateLen;
	uint32_t lampId;
	uint32_t numLEDConnected;
	uint8_t ledPin;
	uint8_t brightness;
	uint8_t opMode;
	uint8_t showDebug;
};

struct lamp_state_t {
	bool opModeChanged;
	uint32_t currentFrameId;
	uint64_t validChunksReceived;
	uint64_t nextChunkReq;
	uint32_t lastWriteIdx;
	unsigned long lastFrameTime;
	struct request_frames_message reqSent;
	struct request_frames_message reqRecv;
	uint32_t reqSentTime;
	uint32_t bootTime;
	uint32_t initPhase;
};
struct lamp_net_state_t {
	bool ifUp;
	bool tcpConnected;
	uint32_t receiveBufPos;
	uint32_t packetsReceived;
	unsigned long lastTcpPoll;
	unsigned long lastTcpReceived;
	unsigned long lastReceived;
	unsigned long lastUdpReceived;
	unsigned long lastUdpHeartBeatSent;
	unsigned long lastTcpHeartBeatSent;
	unsigned long lastTcpConnectAttempt;
};

