#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// OrbitCamera::update
// Purpose: Recompute the camera's world-space position and forward direction from the
//          current focus point, distance, azimuth, and elevation angles.
// Actions: Converts spherical coordinates (azimuth, elevation, distance) to a Cartesian
//          offset from focus and normalizes the forward vector toward focus.
void OrbitCamera::update() {
    float ce = std::cos(elevation), se = std::sin(elevation);
    float ca = std::cos(azimuth),   sa = std::sin(azimuth);
    glm::vec3 offset(ce * ca, se, ce * sa);
    position = focus + offset * distance;
    forward = glm::normalize(focus - position);
}

// OrbitCamera::getView
// Purpose: Build and return a view matrix for the current camera state.
// Returns: glm::lookAt matrix from position toward focus with world +Y as up.
glm::mat4 OrbitCamera::getView() const {
    return glm::lookAt(position, focus, glm::vec3(0.0f, 1.0f, 0.0f));
}

// OrbitCamera::getProj
// Purpose: Build and return a perspective projection matrix.
// Inputs:  aspect    - viewport width divided by height
//          nearPlane - near clip plane distance
//          farPlane  - far clip plane distance
// Returns: Perspective projection matrix using the camera's fovDeg field.
glm::mat4 OrbitCamera::getProj(float aspect, float nearPlane, float farPlane) const {
    return glm::perspective(glm::radians(fovDeg), aspect, nearPlane, farPlane);
}

// OrbitCamera::rotate
// Purpose: Rotate the camera around the focus point in response to a mouse drag.
// Inputs:  dx - horizontal drag distance in pixels (positive = right)
//          dy - vertical drag distance in pixels (positive = down)
// Actions: Adjusts azimuth and elevation by a fixed sensitivity factor, then clamps
//          elevation to +/-89 degrees to prevent gimbal flip.
void OrbitCamera::rotate(float dx, float dy) {
    const float rotationSensitivity = 0.006f;
    azimuth   -= dx * rotationSensitivity;
    elevation += dy * rotationSensitivity;
    const float elevationLimit = glm::radians(89.0f);
    elevation = std::clamp(elevation, -elevationLimit, elevationLimit);
    update();
}

// OrbitCamera::zoom
// Purpose: Zoom the camera in or out by changing its distance from the focus point.
// Inputs:  scrollTicks - scroll wheel input (positive = zoom in, negative = zoom out)
// Actions: Multiplies distance by 1.15 per tick (exponential scaling), then clamps to
//          [minDistance, maxDistance] and calls update().
void OrbitCamera::zoom(float scrollTicks) {
    distance *= std::pow(1.15f, -scrollTicks);
    distance = std::clamp(distance, minDistance, maxDistance);
    update();
}

// OrbitCamera::setFocus
// Purpose: Reposition the camera to orbit a new focus point at a given distance,
//          optionally resetting the viewing angles.
// Inputs:  newFocus     - world-space point the camera will orbit around
//          newDistance  - desired distance from focus (clamped to [min, max])
//          resetAngles  - if true, overwrite azimuth and elevation with azDeg/elDeg
//          azDeg        - new azimuth in degrees (used only when resetAngles = true)
//          elDeg        - new elevation in degrees (used only when resetAngles = true)
void OrbitCamera::setFocus(const glm::vec3& newFocus, float newDistance,
                            bool resetAngles, float azDeg, float elDeg) {
    focus = newFocus;
    distance = std::clamp(newDistance, minDistance, maxDistance);
    if (resetAngles) {
        azimuth   = glm::radians(azDeg);
        elevation = glm::radians(elDeg);
    }
    update();
}
