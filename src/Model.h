#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Shader;

// A loaded glTF model: a flat list of primitives (baked transforms).
// Each primitive has its own VAO/VBO and optional base-color texture.
class Model {
public:
    struct Primitive {
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLsizei indexCount = 0;
        GLenum  indexType = GL_UNSIGNED_INT;
        bool    indexed = false;
        GLsizei vertexCount = 0;     // used if !indexed
        glm::mat4 bakedTransform{1.0f};
        GLuint    baseColorTex = 0;  // 0 = use white fallback
        glm::vec4 baseColorFactor{1.0f};
    };

    std::vector<Primitive> primitives;
    std::vector<GLuint> textures;   // owns GL textures (we'll delete them)
    GLuint whiteTex = 0;            // fallback white texture
    bool loaded = false;

    // Approximate bounding radius in model units, useful for auto-scaling
    float modelRadius = 1.0f;

    ~Model();

    // Load a .glb or .gltf file. Returns true on success.
    bool loadFromFile(const std::string& path, bool flipUVs = false);

    // Draw all primitives. The caller sets uModel/uView/uProj/lighting
    // uniforms on the planet shader BEFORE this; this binds per-primitive
    // transforms as (parentModel * bakedTransform) and the texture.
    void draw(Shader& planetShader, const glm::mat4& parentModel) const;

private:
    void clear();
};
