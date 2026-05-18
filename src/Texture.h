#pragma once
#include <string>
#include <glad/gl.h>

// Loads a 2D texture from file. If the file can't be opened, returns a
// 1x1 magenta fallback texture so the program keeps running.
GLuint loadTexture(const std::string& path, bool srgb = true);

// Creates a solid-color 1x1 texture (used as fallback)
GLuint makeSolidTexture(unsigned char r, unsigned char g, unsigned char b);
