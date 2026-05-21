#pragma once
#include <string>
#include <glad/gl.h>
#include <glm/glm.hpp>

class Shader {
public:
    GLuint id = 0;

    // Compile and link a shader program from vertex and fragment source files.
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Bind this shader program for subsequent draw calls.
    void use() const;
    // Upload an integer uniform to the shader by name.
    void setInt(const std::string& uniformName, int value) const;
    // Upload a float uniform to the shader by name.
    void setFloat(const std::string& uniformName, float value) const;
    // Upload a vec3 uniform to the shader by name.
    void setVec3(const std::string& uniformName, const glm::vec3& value) const;
    // Upload a vec4 uniform to the shader by name.
    void setVec4(const std::string& uniformName, const glm::vec4& value) const;
    // Upload a 3x3 matrix uniform to the shader by name.
    void setMat3(const std::string& uniformName, const glm::mat3& matrix) const;
    // Upload a 4x4 matrix uniform to the shader by name.
    void setMat4(const std::string& uniformName, const glm::mat4& matrix) const;
};
