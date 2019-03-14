#pragma once

template <typename T>
class SafeRefHandler {
public:
	SafeRefHandler() = default;
	virtual ~SafeRefHandler() {

	}
	virtual int safeRefCreate(T*) = 0;
	virtual T* safeRefGet(int32_t refId) = 0;
	virtual void safeRefDestroy(int32_t refId) = 0;
};
template <typename T>
struct SafeRef {
	int refId = -1;
	SafeRefHandler<T>* handler = nullptr;
};
