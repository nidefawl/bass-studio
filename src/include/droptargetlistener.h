#pragma once
#include "str_util.h"
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
using glm::vec2;
using glm::ivec2;


class DropTargetListener {
public:
	virtual ~DropTargetListener() { }
    virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos) = 0;
    virtual bool filesDropMove(ivec2 pos) = 0;
    virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos) = 0;
};
