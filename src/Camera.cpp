#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void OrbitCamera::update() {
    float ce = std::cos(elevation), se = std::sin(elevation);
    float ca = std::cos(azimuth),   sa = std::sin(azimuth);
    glm::vec3 offset(ce * ca, se, ce * sa);
    position = focus + offset * distance;
    forward = glm::normalize(focus - position);
}

glm::mat4 OrbitCamera::getView() const {
    return glm::lookAt(position, focus, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 OrbitCamera::getProj(float aspect, float nearP, float farP) const {
    return glm::perspective(glm::radians(fovDeg), aspect, nearP, farP);
}

void OrbitCamera::rotate(float dx, float dy) {
    const float sens = 0.006f;
    azimuth   -= dx * sens;
    elevation += dy * sens;
    const float lim = glm::radians(89.0f);
    elevation = std::clamp(elevation, -lim, lim);
    update();
}

void OrbitCamera::zoom(float scrollTicks) {
    // Positive scroll (wheel up) = zoom in
    distance *= std::pow(1.15f, -scrollTicks);
    distance = std::clamp(distance, minDistance, maxDistance);
    update();
}

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
