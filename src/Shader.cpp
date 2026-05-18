#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

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

Shader::~Shader() {
    if (id) glDeleteProgram(id);
}

void Shader::use() const { glUseProgram(id); }

void Shader::setInt(const std::string& n, int v) const {
    glUniform1i(glGetUniformLocation(id, n.c_str()), v);
}
void Shader::setFloat(const std::string& n, float v) const {
    glUniform1f(glGetUniformLocation(id, n.c_str()), v);
}
void Shader::setVec3(const std::string& n, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(id, n.c_str()), 1, &v[0]);
}
void Shader::setVec4(const std::string& n, const glm::vec4& v) const {
    glUniform4fv(glGetUniformLocation(id, n.c_str()), 1, &v[0]);
}
void Shader::setMat3(const std::string& n, const glm::mat3& m) const {
    glUniformMatrix3fv(glGetUniformLocation(id, n.c_str()), 1, GL_FALSE, &m[0][0]);
}
void Shader::setMat4(const std::string& n, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(id, n.c_str()), 1, GL_FALSE, &m[0][0]);
}
