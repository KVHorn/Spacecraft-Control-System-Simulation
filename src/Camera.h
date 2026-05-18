#pragma once
#include <glm/glm.hpp>

// Orbit-style camera that rotates around a focus point.
// Right-click + drag rotates; scroll wheel zooms. No free flight.
class OrbitCamera {
public:
    glm::vec3 focus{0.0f};
    float distance  = 1500.0f;
    float azimuth   = 0.0f;          // radians, around world +Y
    float elevation = 0.78539816f;   // radians, 45° by default
    float fovDeg    = 60.0f;
    float minDistance = 1.0f;
    float maxDistance = 1.0e9f;

    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    OrbitCamera() { update(); }

    void update();
    glm::mat4 getView() const;
    glm::mat4 getProj(float aspect, float nearP, float farP) const;

    void rotate(float dxPixels, float dyPixels);
    void zoom(float scrollTicks);

    void setFocus(const glm::vec3& newFocus, float newDistance,
                  bool resetAngles = false,
                  float azDeg = 0.0f, float elDeg = 45.0f);
};
