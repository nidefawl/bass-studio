#pragma once

template <typename T>
class SafeRefHandler {
public:
	SafeRefHandler() = default;
	virtual ~SafeRefHandler() {

	}
	virtual int safeRefCreate(T*) = 0;
	virtual T* safeRefGetPtr(int32_t refId) = 0;
	virtual void safeRefDestroy(int32_t refId) = 0;
};
template <typename T>
struct SafeRef {
	int refId = -1;
	SafeRefHandler<T>* handler = nullptr; // lifetime of handler must exceed refs lifetime
};
template <typename T>
inline bool safeRefOk(SafeRef<T> ref) {
	return ref.handler && ref.handler->safeRefGetPtr(ref.refId) != nullptr;
}
template <typename T>
inline T* safeRefGet(SafeRef<T> ref) {
	if (ref.handler)
		return ref.handler->safeRefGetPtr(ref.refId);
	return nullptr;
}
