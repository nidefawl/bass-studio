#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

template<typename T>
class SafeRefHandler {
public:
    SafeRefHandler()                        = default;
    virtual ~SafeRefHandler()               = default;
    virtual int safeRefCreate(T*)           = 0;
    virtual T* safeRefGetPtr(int32_t refId) = 0;


    /**
     * this should be called before delete
     * calling it in the base class destructor is not safe
     */
    virtual void safeRefDestroy(int32_t refId) = 0;
};

template<typename T>
class SafeRefStorage : public SafeRefHandler<T> {
    struct RefStored {
        T* ptr;
        int32_t refId;
    };
    int32_t refIdNext = 0;
    std::vector<RefStored> refs;

public:
    SafeRefStorage() = default;
    virtual ~SafeRefStorage() = default;
    int safeRefCreate(T* gui) override {
        RefStored ref{ gui, (int32_t) refIdNext++ };
        refs.push_back(ref);
        return ref.refId;
    }
    T* safeRefGetPtr(int32_t refId) override {
        auto it = std::find_if(refs.begin(), refs.end(), [refId](const RefStored& ref) {
            return ref.refId == refId;
        });
        if (it != refs.end()) {
            RefStored& ref = *it;
            return ref.ptr;
        }
        return nullptr;
    }
    void safeRefDestroy(int32_t refId) override {
        auto it = std::find_if(refs.begin(), refs.end(), [refId](const RefStored& ref) {
            return ref.refId == refId;
        });
        if (it != refs.end()) {
            it->ptr = nullptr;
            refs.erase(it);
        }
    }
};
template<typename T>
struct SafeRef {
    int refId = -1;
    SafeRefHandler<T>* handler = nullptr;// lifetime of handler must exceed refs lifetime
};
template<typename T>
inline bool safeRefOk(SafeRef<T> ref) {
    return ref.handler && ref.handler->safeRefGetPtr(ref.refId) != nullptr;
}
template<typename T>
inline T* safeRefGet(SafeRef<T> ref) {
    if (ref.handler)
        return ref.handler->safeRefGetPtr(ref.refId);
    return nullptr;
}
