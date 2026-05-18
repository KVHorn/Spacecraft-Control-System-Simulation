#include "Texture.h"
#include <stb_image.h>
#include <iostream>

GLuint makeSolidTexture(unsigned char r, unsigned char g, unsigned char b) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char pixel[3] = { r, g, b };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

GLuint loadTexture(const std::string& path, bool srgb) {
    stbi_set_flip_vertically_on_load(true);
    int w, h, comp;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 0);
    if (!data) {
        std::cerr << "[Texture] Failed to load \"" << path
                  << "\" (" << stbi_failure_reason() << ") - using magenta fallback\n";
        return makeSolidTexture(255, 0, 255);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum dataFmt, internalFmt;
    if (comp == 4) {
        dataFmt = GL_RGBA;
        internalFmt = srgb ? GL_SRGB_ALPHA : GL_RGBA;
    } else if (comp == 3) {
        dataFmt = GL_RGB;
        internalFmt = srgb ? GL_SRGB : GL_RGB;
    } else if (comp == 1) {
        dataFmt = GL_RED;
        internalFmt = GL_RED;
    } else {
        std::cerr << "[Texture] Unsupported channel count " << comp << " for " << path << "\n";
        stbi_image_free(data);
        return makeSolidTexture(255, 0, 255);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, dataFmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Optional: anisotropic filtering if supported
    GLfloat maxAniso = 1.0f;
    glGetFloatv(0x84FF /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */, &maxAniso);
    if (maxAniso > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */,
                        (maxAniso > 8.0f) ? 8.0f : maxAniso);
    }

    stbi_image_free(data);
    std::cout << "[Texture] Loaded " << path << " (" << w << "x" << h << ", " << comp << " ch)\n";
    return tex;
}
