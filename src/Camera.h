#pragma once

#include "Math.h"

class Camera {
public:
    Camera() { reset(); }

    void setFramingRadius(float radius) {
        defaultDistance_ = og::clampf(radius * 4.3f, 4.8f, 18.0f);
        distance_ = defaultDistance_;
    }

    void reset() {
        target_ = {0.0f, 0.02f, 0.0f};
        yaw_ = og::radians(34.0f);
        pitch_ = og::radians(23.0f);
        distance_ = defaultDistance_;
    }

    og::Vec3 position() const {
        const float cp = std::cos(pitch_);
        return target_ + og::Vec3(std::sin(yaw_) * cp, std::sin(pitch_), std::cos(yaw_) * cp) * distance_;
    }

    og::Mat4 view() const {
        return og::lookAt(position(), target_, og::Vec3(0.0f, 1.0f, 0.0f));
    }

    og::Vec3 forward() const { return og::normalize(target_ - position()); }
    og::Vec3 right() const { return og::normalize(og::cross(forward(), og::Vec3(0.0f, 1.0f, 0.0f))); }
    og::Vec3 up() const { return og::normalize(og::cross(right(), forward())); }

    og::Mat4 projection(float aspect) const {
        return og::perspective(og::radians(42.0f), aspect, 0.05f, 120.0f);
    }

    void orbit(float dxPixels, float dyPixels) {
        yaw_ -= dxPixels * 0.0055f;
        pitch_ = og::clampf(pitch_ + dyPixels * 0.0055f, og::radians(-84.0f), og::radians(84.0f));
    }

    void pan(float dxPixels, float dyPixels) {
        const og::Vec3 forward = og::normalize(target_ - position());
        const og::Vec3 right = og::normalize(og::cross(forward, og::Vec3(0.0f, 1.0f, 0.0f)));
        const og::Vec3 up = og::normalize(og::cross(right, forward));
        const float speed = distance_ * 0.00135f;
        target_ += right * (-dxPixels * speed) + up * (dyPixels * speed);
    }

    void zoom(float wheelSteps) {
        distance_ = og::clampf(distance_ * std::pow(0.87f, wheelSteps), 1.25f, 26.0f);
    }

    void setAxisView(int axis, int sign) {
        if (axis == 0) {
            yaw_ = sign >= 0 ? og::kPi * 0.5f : -og::kPi * 0.5f;
            pitch_ = 0.0f;
        } else if (axis == 1) {
            yaw_ = 0.0f;
            pitch_ = sign >= 0 ? og::radians(88.0f) : og::radians(-88.0f);
        } else {
            yaw_ = sign >= 0 ? 0.0f : og::kPi;
            pitch_ = 0.0f;
        }
    }

    const og::Vec3& target() const { return target_; }
    float distance() const { return distance_; }
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }

private:
    og::Vec3 target_;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float distance_ = 5.2f;
    float defaultDistance_ = 5.2f;
};
