#pragma once
#include "types.h"
#include <map>
#include "str_util.h"

struct win32_hwnd_msg_counter_t {
#define DBG_WIN32_HWND_MSG_ARR_LEN 10000
    struct data_t {
        int id;
        int count;
    };
    int maxIdx = 0;
    struct data_t messages[DBG_WIN32_HWND_MSG_ARR_LEN] = {};
    std::map<String, int> hwndPaints;
    void incrMessage(int id) {
        for (int i = 0; i < maxIdx; i++) {
            int storedId = messages[i].id;
            if (storedId == id) {
                messages[i].count++;
                return;
            }
        }
        messages[maxIdx].id = id;
        messages[maxIdx].count++;
        maxIdx++;
    }
    void incrPaints(const char* wndClassName) {
        if (hwndPaints.count(wndClassName)) {
            hwndPaints[wndClassName] = hwndPaints.at(wndClassName) + 1;
        } else {
            hwndPaints[wndClassName] = 1;
        }
    }
    int getNumMsg() const {
        return maxIdx;
    }
    int getMsgId(int i) {
        return messages[i].id;
    }
    int getMsgCnt(int i) {
        return messages[i].count;
    }
    int getHWNDMapSize() {
        return (int)hwndPaints.size();
    }
    String getHWNDName(int i) {
        auto it = hwndPaints.begin();
        for (int j = 0; j < i; j++, it++)
            ;
        return it->first;
    }
    int getHWNDCnt(int i) {
        auto it = hwndPaints.begin();
        for (int j = 0; j < i; j++, it++)
            ;
        return it->second;
    }
};
extern win32_hwnd_msg_counter_t msgCounter;
extern bool msgCounterEnabled;
