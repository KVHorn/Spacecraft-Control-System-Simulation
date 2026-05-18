#include "Mesh.h"
#include <vector>
#include <cmath>

static constexpr float PI_F = 3.14159265358979323846f;

// ---------------- SphereMesh ----------------
SphereMesh::SphereMesh(int segments, int rings) {
    std::vector<float> v;
    std::vector<unsigned int> idx;
    v.reserve((segments + 1) * (rings + 1) * 8);

    for (int y = 0; y <= rings; ++y) {
        for (int x = 0; x <= segments; ++x) {
            float u = float(x) / segments;
            float t = float(y) / rings;
            float theta = u * 2.0f * PI_F;
            float phi = t * PI_F;

            float sx = std::cos(theta) * std::sin(phi);
            float sy = std::cos(phi);
            float sz = std::sin(theta) * std::sin(phi);

            v.push_back(sx); v.push_back(sy); v.push_back(sz);
            v.push_back(sx); v.push_back(sy); v.push_back(sz);
            v.push_back(u);  v.push_back(1.0f - t);
        }
    }

    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            int i0 = y * (segments + 1) + x;
            int i1 = i0 + 1;
            int i2 = i0 + (segments + 1);
            int i3 = i2 + 1;
            idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
            idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
        }
    }
    indexCount = (GLsizei)idx.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

SphereMesh::~SphereMesh() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}

void SphereMesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

// ---------------- RingMesh ----------------
RingMesh::RingMesh(float innerRadius, float outerRadius, int segments) {
    std::vector<float> v;
    std::vector<unsigned int> idx;
    v.reserve((segments + 1) * 16);

    for (int i = 0; i <= segments; ++i) {
        float a = (float)i / segments * 2.0f * PI_F;
        float cx = std::cos(a), sz = std::sin(a);

        // Inner ring vertex (uv.x = 0)
        v.push_back(cx * innerRadius); v.push_back(0.0f); v.push_back(sz * innerRadius);
        v.push_back(0.0f); v.push_back(1.0f); v.push_back(0.0f);
        v.push_back(0.0f); v.push_back(0.5f);

        // Outer ring vertex (uv.x = 1)
        v.push_back(cx * outerRadius); v.push_back(0.0f); v.push_back(sz * outerRadius);
        v.push_back(0.0f); v.push_back(1.0f); v.push_back(0.0f);
        v.push_back(1.0f); v.push_back(0.5f);
    }

    for (int i = 0; i < segments; ++i) {
        int i0 = i * 2, i1 = i0 + 1, i2 = i0 + 2, i3 = i0 + 3;
        idx.push_back(i0); idx.push_back(i1); idx.push_back(i2);
        idx.push_back(i2); idx.push_back(i1); idx.push_back(i3);
    }
    indexCount = (GLsizei)idx.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

RingMesh::~RingMesh() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}

void RingMesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

// ---------------- OrbitMesh ----------------
OrbitMesh::OrbitMesh(float radius, int segments) {
    std::vector<float> v;
    v.reserve(segments * 3);
    for (int i = 0; i < segments; ++i) {
        float a = (float)i / segments * 2.0f * PI_F;
        v.push_back(std::cos(a) * radius);
        v.push_back(0.0f);
        v.push_back(std::sin(a) * radius);
    }
    vertexCount = (GLsizei)(v.size() / 3);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

OrbitMesh::~OrbitMesh() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
}

void OrbitMesh::draw() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_LOOP, 0, vertexCount);
}

// ---------------- SkyboxMesh ----------------
SkyboxMesh::SkyboxMesh() {
    // 36 vertices of a unit cube (no indices, positions only)
    const float verts[] = {
        -1,  1, -1,  -1, -1, -1,   1, -1, -1,
         1, -1, -1,   1,  1, -1,  -1,  1, -1,
        -1, -1,  1,  -1, -1, -1,  -1,  1, -1,
        -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
         1, -1, -1,   1, -1,  1,   1,  1,  1,
         1,  1,  1,   1,  1, -1,   1, -1, -1,
        -1, -1,  1,  -1,  1,  1,   1,  1,  1,
         1,  1,  1,   1, -1,  1,  -1, -1,  1,
        -1,  1, -1,   1,  1, -1,   1,  1,  1,
         1,  1,  1,  -1,  1,  1,  -1,  1, -1,
        -1, -1, -1,  -1, -1,  1,   1, -1, -1,
         1, -1, -1,  -1, -1,  1,   1, -1,  1
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

SkyboxMesh::~SkyboxMesh() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
}

void SkyboxMesh::draw() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}
