#pragma once
#include <GLFW/glfw3.h>

class GlfwContextSwitch {
    GLFWwindow* const window;
    GLFWwindow* const curContext;
public:
    GlfwContextSwitch(GLFWwindow* window) : window(window), curContext(glfwGetCurrentContext()) {
        if (curContext != window) {
            glfwMakeContextCurrent(window);
        }
    }
    ~GlfwContextSwitch() {
        if (curContext != window) {
            glfwMakeContextCurrent(curContext);
        }
    }
};