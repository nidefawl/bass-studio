#pragma once
#ifndef GLFWwindow
struct GLFWwindow;
#endif
void sendExposeEvent(GLFWwindow* glfw);
void setIsTransientFor(GLFWwindow* glfw, GLFWwindow* glfwChild);
