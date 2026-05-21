#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

// readFile
// Purpose: Read the entire contents of a text file into a string.
// Inputs:  path - file path to open
// Returns: File contents as a string, or empty string if the file could not be opened.
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[Shader] Failed to open: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// compileStage
// Purpose: Compile a single OpenGL shader stage (vertex or fragment).
// Inputs:  type  - GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
//          src   - GLSL source code string
//          label - filename or identifier used in error messages
// Returns: OpenGL shader object handle. Logs a compile error if compilation fails.
static GLuint compileStage(GLenum type, const std::string& src, const std::string& label) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Compile error in " << label << ":\n" << log << std::endl;
    }
    return s;
}

// Shader::Shader
// Purpose: Compile vertex and fragment shader source files and link them into a program.
// Inputs:  vertPath - path to the GLSL vertex shader source file
//          fragPath - path to the GLSL fragment shader source file
// Actions: Reads both source files, compiles each stage, links into a program stored in id,
//          then deletes the intermediate shader objects. Logs errors on failure.
Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    std::string vsrc = readFile(vertPath);
    std::string fsrc = readFile(fragPath);

    GLuint vs = compileStage(GL_VERTEX_SHADER, vsrc, vertPath);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsrc, fragPath);

    id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);

    GLint ok = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(id, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Link error (" << vertPath << " + " << fragPath << "):\n"
                  << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// Shader::~Shader
// Purpose: Delete the OpenGL shader program when the Shader object is destroyed.
Shader::~Shader() {
    if (id) glDeleteProgram(id);
}

// Shader::use
// Purpose: Bind this shader program as the active OpenGL program for subsequent draws.
void Shader::use() const { glUseProgram(id); }

// Shader::setInt
// Purpose: Set an integer uniform in the shader program.
// Inputs:  uniformName - GLSL uniform variable name
//          value       - integer value to upload
void Shader::setInt(const std::string& uniformName, int value) const {
    glUniform1i(glGetUniformLocation(id, uniformName.c_str()), value);
}

// Shader::setFloat
// Purpose: Set a float uniform in the shader program.
// Inputs:  uniformName - GLSL uniform variable name
//          value       - float value to upload
void Shader::setFloat(const std::string& uniformName, float value) const {
    glUniform1f(glGetUniformLocation(id, uniformName.c_str()), value);
}

// Shader::setVec3
// Purpose: Set a vec3 uniform in the shader program.
// Inputs:  uniformName - GLSL uniform variable name
//          value       - 3-component float vector to upload
void Shader::setVec3(const std::string& uniformName, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(id, uniformName.c_str()), 1, &value[0]);
}

// Shader::setVec4
// Purpose: Set a vec4 uniform in the shader program.
// Inputs:  uniformName - GLSL uniform variable name
//          value       - 4-component float vector to upload
void Shader::setVec4(const std::string& uniformName, const glm::vec4& value) const {
    glUniform4fv(glGetUniformLocation(id, uniformName.c_str()), 1, &value[0]);
}

// Shader::setMat3
// Purpose: Set a mat3 uniform in the shader program.
// Inputs:  uniformName - GLSL uniform variable name
//          matrix      - 3x3 column-major float matrix to upload
void Shader::setMat3(const std::string& uniformName, const glm::mat3& matrix) const {
    glUniformMatrix3fv(glGetUniformLocation(id, uniformName.c_str()), 1, GL_FALSE, &matrix[0][0]);
}

// Shader::setMat4
// Purpose: Set a mat4 uniform in the shader program.
// Inputs:  uniformName - GLSL uniform variable name
//          matrix      - 4x4 column-major float matrix to upload
void Shader::setMat4(const std::string& uniformName, const glm::mat4& matrix) const {
    glUniformMatrix4fv(glGetUniformLocation(id, uniformName.c_str()), 1, GL_FALSE, &matrix[0][0]);
}
