#pragma once
#include <glad/gl.h>

// UV sphere mesh (radius 1). Pos/Normal/UV attributes at locations 0/1/2.
class SphereMesh {
public:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    // Build a UV sphere with the given number of longitude segments and latitude rings.
    SphereMesh(int segments = 64, int rings = 48);
    ~SphereMesh();
    SphereMesh(const SphereMesh&) = delete;
    SphereMesh& operator=(const SphereMesh&) = delete;
    // Issue a GL_TRIANGLES draw call using this mesh's VAO.
    void draw() const;
};

// Flat annular ring (disc with a hole). In local space it lies in the XZ plane.
// UV.x goes from 0 (inner) to 1 (outer), so a radial ring texture maps naturally.
class RingMesh {
public:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    // Build an annular ring mesh between innerRadius and outerRadius using the given segment count.
    RingMesh(float innerRadius, float outerRadius, int segments = 256);
    ~RingMesh();
    RingMesh(const RingMesh&) = delete;
    RingMesh& operator=(const RingMesh&) = delete;
    // Issue a GL_TRIANGLES draw call using this mesh's VAO.
    void draw() const;
};

// Circle of line segments in the XZ plane (for drawing orbit paths).
class OrbitMesh {
public:
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
    // Build a circle line loop with the given radius and number of segments.
    OrbitMesh(float radius = 1.0f, int segments = 256);
    ~OrbitMesh();
    OrbitMesh(const OrbitMesh&) = delete;
    OrbitMesh& operator=(const OrbitMesh&) = delete;
    // Issue a GL_LINE_LOOP draw call using this mesh's VAO.
    void draw() const;
};

// Inward-facing cube for rendering a skybox.
class SkyboxMesh {
public:
    GLuint vao = 0, vbo = 0;
    // Build a unit cube with inward-facing winding order for use as a skybox.
    SkyboxMesh();
    ~SkyboxMesh();
    SkyboxMesh(const SkyboxMesh&) = delete;
    SkyboxMesh& operator=(const SkyboxMesh&) = delete;
    // Issue a GL_TRIANGLES draw call for all 36 skybox vertices.
    void draw() const;
};
