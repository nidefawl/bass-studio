#pragma once
#include <stdlib.h>

template<typename T>
struct ref {
	T* ref;
public:
	void onDelete() {
		ref = nullptr;
	}
	T get() {
		return ref;
	}
};
