#pragma once
#include "types.h"
#include <vector>
#include <algorithm>

template<typename T>
class SafeRefHandler {
protected:
    ~SafeRefHandler() = default;

public:
    SafeRefHandler()                        = default;
    virtual size_t safeRefCreate(T*)           = 0;
    virtual T* safeRefGetPtr(size_t refId) = 0;
    virtual const T* safeRefGetPtr(size_t refId) const = 0;
    virtual void safeRefDestroy(size_t refId) = 0;
};

template<typename T>
class SafeRefStorage : public SafeRefHandler<T> {
    struct RefStored {
        T* ptr;
        size_t refId;
    };
    size_t refIdNext = 0;
    std::vector<RefStored*> refs;

public:
    SafeRefStorage() = default;
    virtual ~SafeRefStorage() {
        for (auto& ref : refs) {
            if (ref->ptr)
                ref->ptr->safeRef.handler = nullptr;
            ref->ptr = nullptr;
            delete ref;
        }
    }
    size_t safeRefCreate(T* gui) override {
        auto ref = new RefStored{ gui, refIdNext++ };
        // insert sorted so binary_search works
        auto it = std::lower_bound(refs.begin(), refs.end(), ref, [](auto& a, auto& b) {
            return a->refId < b->refId;
        });
        refs.insert(it, ref);
        return reinterpret_cast<size_t>(ref);
    }
    T* safeRefGetPtr(size_t refId) override {
        auto ref = reinterpret_cast<RefStored*>(refId);
        if (ref->ptr) {
            return ref->ptr;
        }
        return nullptr;
    }
    const T* safeRefGetPtr(size_t refId) const override {
        auto ref = reinterpret_cast<const RefStored*>(refId);
        if (ref->ptr) {
            return ref->ptr;
        }
        return nullptr;
    }
    void safeRefDestroy(size_t refId) override {
        auto ref = reinterpret_cast<RefStored*>(refId);
        if (ref->ptr) {
            ref->ptr = nullptr;
        }
    }
};
template<typename T>
struct SafeRef {
    size_t refId = -1;
    SafeRefHandler<T>* handler = nullptr;// lifetime of handler must exceed refs lifetime
    bool isEmpty() const {
        return refId == -1;
    }
};
template<typename T>
bool operator==(const SafeRef<T>& a, const SafeRef<T>& b) {
    return a.refId == b.refId;
}
template<typename T>
struct StoredReference {
    T* ptr;
    size_t refId;
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
