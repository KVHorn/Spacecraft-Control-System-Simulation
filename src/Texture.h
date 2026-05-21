#pragma once
#include <string>
#include <glad/gl.h>

// Load a 2D texture from a file with mipmaps and anisotropic filtering.
// srgb=true applies gamma-correct GL_SRGB internal format. Returns a 1x1
// magenta fallback texture if the file cannot be opened.
GLuint loadTexture(const std::string& path, bool srgb = true);

// Create a 1x1 solid-color GL texture. Used as a placeholder or fallback
// when a real texture is unavailable.
GLuint makeSolidTexture(unsigned char r, unsigned char g, unsigned char b);
