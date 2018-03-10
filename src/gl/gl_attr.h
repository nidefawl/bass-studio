#pragma once
#include <vector>

struct VertexAttr {
	const char* name;
	int elements;
	int type;
	int bindingPt = 0;
};

void bindVertexAttributes(std::vector<VertexAttr>& attr);
