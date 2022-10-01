#pragma once
#include <vector>
#include "types.h"
#include "str_util.h"
#include "assert_dbg.h"

#define TRACK_TYPE_MASTER 0
#define TRACK_TYPE_RETURN 1
#define TRACK_TYPE_MIDI 2
#define TRACK_TYPE_AUDIO 3
#define NUM_TRACK_TYPES 4
#define TRACK_CTR_MASTER 0
#define TRACK_CTR_RETURN 1
#define TRACK_CTR_MIDIAUDIO 2
#define NUM_TRACK_TYPE_CTRS 4

template<typename T>
T TRACKTYPE_TO_CTR(const T trackType) {
    switch (trackType) {
        case TRACK_TYPE_MASTER:
            return TRACK_CTR_MASTER;
        case TRACK_TYPE_RETURN:
            return TRACK_CTR_RETURN;
        case TRACK_TYPE_MIDI:
        case TRACK_TYPE_AUDIO:
            return TRACK_CTR_MIDIAUDIO;
    }
    dbgassert(0);
    return -1;
}

#define FLG_TRK_CHANGE_USER 1
#define FLG_TRK_CHANGE_LOAD 2
#define FLG_TRK_CHANGE_HISTORY_UNDO 4
const char* TrackTypeToName(int type);

enum class audiostageflags_t : int32_t {
    NONE                  = 0,
    SOLO                  = 1 << 1,
    SOLO_PARENT           = 1 << 2,
    RECORD_ARMED          = 1 << 3,
    RECORD_OUTPUT         = 1 << 4,
    CONVERT_OUTPUT        = 1 << 5,
};

enum class audiostagerouting_state_t : int32_t {
    INVALID = 0,
    DEFAULT = 1,
    CUSTOM  = 2
};

template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T operator~(T a) { return (T) ~(int32_t) a; }
template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T operator|(T a, T b) { return (T) ((int32_t) a | (int32_t) b); }
template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T operator&(T a, T b) { return (T) ((int32_t) a & (int32_t) b); }
template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T operator^(T a, T b) { return (T) ((int32_t) a ^ (int32_t) b); }
template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T& operator|=(T& a, T b) { return (T&) ((int32_t&) a |= (int32_t) b); }
template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T& operator&=(T& a, T b) { return (T&) ((int32_t&) a &= (int32_t) b); }
template<class T, typename = std::enable_if_t<std::is_same<T, audiostageflags_t>::value>>
inline T& operator^=(T& a, T b) { return (T&) ((int32_t&) a ^= (int32_t) b); }

template<class T, typename = audiostageflags_t>
inline bool isSet(T a, T b) { return static_cast<bool>((a & b) == b); }

struct tracksettings_t {
    String name = "INVALID";
    int type    = -1;//CONST!
    int rgb     = -1;
};

struct tracklayout_settings_t {
    int height         = 4;
    bool hideTrack     = false;
    bool hideSubtracks = false;
};

struct subtracksettings_t {
    int32_t subtrackType    = 0;
};

struct subtracklayout_settings_t {
    int height         = 4;
};