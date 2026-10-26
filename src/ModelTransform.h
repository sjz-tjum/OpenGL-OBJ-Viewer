#pragma once

#include "Math.h"

struct ModelTransform {
    og::Vec3 translation{0.0f, 0.0f, 0.0f};
    og::Vec3 rotation{0.0f, 0.0f, 0.0f};
    float scale = 1.0f;

    og::Mat4 matrix() const {
        return og::translation(translation)
             * og::rotationZ(rotation.z)
             * og::rotationY(rotation.y)
             * og::rotationX(rotation.x)
             * og::scaling({scale, scale, scale});
    }

    void reset() {
        translation = {0.0f, 0.0f, 0.0f};
        rotation = {0.0f, 0.0f, 0.0f};
        scale = 1.0f;
    }
};
