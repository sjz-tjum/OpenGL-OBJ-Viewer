#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Math.h"

struct Material {
    std::string name = "default";
    og::Vec3 diffuse{0.72f, 0.74f, 0.78f};
    og::Vec3 specular{0.45f, 0.45f, 0.45f};
    og::Vec3 emissive{0.0f, 0.0f, 0.0f};
    std::string diffuseMap;
    bool diffuseSpecified = false;
    float shininess = 32.0f;
    float opacity = 1.0f;
};

struct MeshVertex {
    og::Vec3 position;
    og::Vec3 normal;
    og::Vec2 texcoord;
};

struct DrawRange {
    int material = 0;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
};

class ObjModel {
public:
    bool load(const std::string& path, std::string& error);

    const std::vector<MeshVertex>& vertices() const { return vertices_; }
    const std::vector<std::uint32_t>& indices() const { return indices_; }
    const std::vector<Material>& materials() const { return materials_; }
    const std::vector<DrawRange>& ranges() const { return ranges_; }

    const std::string& name() const { return name_; }
    const std::string& path() const { return path_; }
    std::uint32_t triangleCount() const { return static_cast<std::uint32_t>(indices_.size() / 3); }
    float floorY() const { return floorY_; }
    float boundingRadius() const { return boundingRadius_; }

private:
    void parseMtl(const std::string& path);
    int ensureMaterial(const std::string& name);

    std::vector<MeshVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<Material> materials_;
    std::vector<DrawRange> ranges_;
    std::string name_;
    std::string path_;
    float floorY_ = -1.0f;
    float boundingRadius_ = 1.2f;
};
