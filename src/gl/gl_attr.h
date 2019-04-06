#pragma once
#include <vector>

struct VertexAttr {
	const char* name;
	int elements;
	int type;
	int bindingPt = 0;
};

/** This is only required to be called once per vao */
void bindVertexAttributes(std::vector<VertexAttr>& attr, int fixedStride = 0);
