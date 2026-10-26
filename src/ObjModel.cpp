#include "ObjModel.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

namespace {

std::string trim(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string directoryOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

std::string baseName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string joinPath(const std::string& directory, const std::string& file) {
    if (file.empty() || file[0] == '/' || (file.size() > 1 && file[1] == ':')) {
        return file;
    }
    return directory + "/" + file;
}


struct PositionKey {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool operator<(const PositionKey& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

struct NormalSample {
    og::Vec3 normal;
    float weight = 0.0f;
};

void smoothNormals(std::vector<MeshVertex>& vertices, const std::vector<std::uint32_t>& indices) {
    std::map<PositionKey, std::vector<NormalSample>> adjacency;

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const MeshVertex& a = vertices[indices[i]];
        const MeshVertex& b = vertices[indices[i + 1]];
        const MeshVertex& c = vertices[indices[i + 2]];
        const og::Vec3 cross = og::cross(b.position - a.position, c.position - a.position);
        const float length = og::length(cross);
        if (length < 1.0e-8f) {
            continue;
        }
        const NormalSample sample{og::Vec3(cross.x / length, cross.y / length, cross.z / length), length};
        for (const std::uint32_t index : {indices[i], indices[i + 1], indices[i + 2]}) {
            const og::Vec3& position = vertices[index].position;
            adjacency[PositionKey{position.x, position.y, position.z}].push_back(sample);
        }
    }

    const float creaseLimit = std::cos(og::radians(40.0f));
    for (MeshVertex& vertex : vertices) {
        const auto found = adjacency.find(PositionKey{vertex.position.x, vertex.position.y, vertex.position.z});
        if (found == adjacency.end()) {
            continue;
        }
        og::Vec3 sum(0.0f, 0.0f, 0.0f);
        for (const NormalSample& sample : found->second) {
            if (og::dot(vertex.normal, sample.normal) >= creaseLimit) {
                sum += sample.normal * sample.weight;
            }
        }
        if (og::length(sum) > 1.0e-8f) {
            vertex.normal = og::normalize(sum);
        }
    }
}

int parseInteger(const std::string& text) {
    if (text.empty()) {
        return -1;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    return end == text.c_str() ? -1 : static_cast<int>(value);
}

struct FaceIndex {
    int position = -1;
    int texcoord = -1;
    int normal = -1;
};

FaceIndex parseFaceIndex(const std::string& token) {
    FaceIndex result;
    const size_t firstSlash = token.find('/');
    if (firstSlash == std::string::npos) {
        result.position = parseInteger(token);
        return result;
    }
    result.position = parseInteger(token.substr(0, firstSlash));
    const size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos) {
        result.texcoord = parseInteger(token.substr(firstSlash + 1));
    } else {
        result.texcoord = parseInteger(token.substr(firstSlash + 1, secondSlash - firstSlash - 1));
        result.normal = parseInteger(token.substr(secondSlash + 1));
    }
    return result;
}

bool resolveIndex(int& index, std::size_t count) {
    if (index > 0) {
        --index;
    } else if (index < 0) {
        index = static_cast<int>(count) + index;
    }
    return index >= 0 && static_cast<std::size_t>(index) < count;
}

} // namespace

int ObjModel::ensureMaterial(const std::string& name) {
    for (std::size_t i = 0; i < materials_.size(); ++i) {
        if (materials_[i].name == name) {
            return static_cast<int>(i);
        }
    }
    Material material;
    material.name = name;
    materials_.push_back(material);
    return static_cast<int>(materials_.size() - 1);
}

void ObjModel::parseMtl(const std::string& mtlPath) {
    std::ifstream file(mtlPath);
    if (!file) {
        return;
    }

    int current = -1;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag == "newmtl") {
            std::string name;
            std::getline(stream, name);
            current = ensureMaterial(trim(name));
        } else if (current < 0) {
            continue;
        } else if (tag == "Kd") {
            stream >> materials_[current].diffuse.x >> materials_[current].diffuse.y
                   >> materials_[current].diffuse.z;
            materials_[current].diffuseSpecified = true;
        } else if (tag == "map_Kd") {
            std::string value;
            std::getline(stream, value);
            value = trim(value);
            if (!value.empty()) {
                materials_[current].diffuseMap = joinPath(directoryOf(mtlPath), value);
            }
        } else if (tag == "Ks") {
            stream >> materials_[current].specular.x >> materials_[current].specular.y
                   >> materials_[current].specular.z;
        } else if (tag == "Ke") {
            stream >> materials_[current].emissive.x >> materials_[current].emissive.y
                   >> materials_[current].emissive.z;
        } else if (tag == "Ns") {
            float value = 32.0f;
            if (stream >> value) {
                materials_[current].shininess = og::clampf(value, 2.0f, 1000.0f);
            }
        } else if (tag == "d") {
            float value = 1.0f;
            if (stream >> value) {
                materials_[current].opacity = og::clampf(value, 0.0f, 1.0f);
            }
        } else if (tag == "Tr") {
            float value = 0.0f;
            if (stream >> value) {
                materials_[current].opacity = og::clampf(1.0f - value, 0.0f, 1.0f);
            }
        }
    }
}

bool ObjModel::load(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "Could not open OBJ file: " + path;
        return false;
    }

    vertices_.clear();
    indices_.clear();
    materials_.clear();
    ranges_.clear();
    path_ = path;
    name_ = baseName(path);

    std::vector<og::Vec3> positions;
    std::vector<og::Vec3> normals;
    std::vector<og::Vec2> texcoords;
    std::map<std::array<std::uint32_t, 3>, std::uint32_t> vertexCache;

    int currentMaterial = -1;
    std::uint32_t rangeStart = 0;
    int rangeMaterial = 0;
    int lineNumber = 0;
    std::string line;

    auto closeRange = [&]() {
        const std::uint32_t count = static_cast<std::uint32_t>(indices_.size()) - rangeStart;
        if (currentMaterial >= 0 && count > 0) {
            ranges_.push_back({rangeMaterial, rangeStart, count});
        }
        rangeStart = static_cast<std::uint32_t>(indices_.size());
    };

    while (std::getline(file, line)) {
        ++lineNumber;
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag.empty() || tag[0] == '#') {
            continue;
        }

        if (tag == "v") {
            og::Vec3 p;
            if (!(stream >> p.x >> p.y >> p.z)) {
                error = "Invalid vertex at line " + std::to_string(lineNumber);
                return false;
            }
            positions.push_back(p);
        } else if (tag == "vt") {
            og::Vec2 uv;
            if (!(stream >> uv.x >> uv.y)) {
                error = "Invalid texture coordinate at line " + std::to_string(lineNumber);
                return false;
            }
            texcoords.push_back(uv);
        } else if (tag == "vn") {
            og::Vec3 n;
            if (!(stream >> n.x >> n.y >> n.z)) {
                error = "Invalid normal at line " + std::to_string(lineNumber);
                return false;
            }
            normals.push_back(og::normalize(n));
        } else if (tag == "mtllib") {
            std::string library;
            while (stream >> library) {
                parseMtl(joinPath(directoryOf(path), library));
            }
        } else if (tag == "usemtl") {
            std::string materialName;
            std::getline(stream, materialName);
            closeRange();
            currentMaterial = ensureMaterial(trim(materialName));
            rangeMaterial = currentMaterial;
        } else if (tag == "f") {
            std::vector<FaceIndex> face;
            std::string token;
            while (stream >> token) {
                face.push_back(parseFaceIndex(token));
            }
            if (face.size() < 3) {
                error = "Face with fewer than three vertices at line " + std::to_string(lineNumber);
                return false;
            }
            for (FaceIndex& index : face) {
                if (!resolveIndex(index.position, positions.size())) {
                    error = "Vertex index out of range at line " + std::to_string(lineNumber);
                    return false;
                }
                if (index.normal >= 0 && !resolveIndex(index.normal, normals.size())) {
                    error = "Normal index out of range at line " + std::to_string(lineNumber);
                    return false;
                }
                if (index.texcoord >= 0 && !resolveIndex(index.texcoord, texcoords.size())) {
                    error = "Texture coordinate index out of range at line " + std::to_string(lineNumber);
                    return false;
                }
            }
            if (currentMaterial < 0) {
                currentMaterial = ensureMaterial("default");
                rangeMaterial = currentMaterial;
                rangeStart = 0;
            }

            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                const FaceIndex corners[3] = {face[0], face[i], face[i + 1]};
                const og::Vec3& p0 = positions[corners[0].position];
                const og::Vec3& p1 = positions[corners[1].position];
                const og::Vec3& p2 = positions[corners[2].position];
                const og::Vec3 faceNormal = og::normalize(og::cross(p1 - p0, p2 - p0));

                for (const FaceIndex& corner : corners) {
                    const og::Vec3& position = positions[corner.position];
                    const og::Vec3 normal = corner.normal >= 0 ? normals[corner.normal] : faceNormal;
                    const og::Vec2 uv = corner.texcoord >= 0 ? texcoords[corner.texcoord] : og::Vec2{0.0f, 0.0f};
                    if (corner.normal >= 0) {
                        const std::array<std::uint32_t, 3> key{
                            static_cast<std::uint32_t>(corner.position),
                            static_cast<std::uint32_t>(corner.normal),
                            corner.texcoord >= 0 ? static_cast<std::uint32_t>(corner.texcoord) : UINT32_MAX};
                        auto found = vertexCache.find(key);
                        if (found != vertexCache.end()) {
                            indices_.push_back(found->second);
                            continue;
                        }
                        const std::uint32_t newIndex = static_cast<std::uint32_t>(vertices_.size());
                        vertices_.push_back({position, normal, uv});
                        vertexCache.emplace(key, newIndex);
                        indices_.push_back(newIndex);
                    } else {
                        indices_.push_back(static_cast<std::uint32_t>(vertices_.size()));
                        vertices_.push_back({position, normal, uv});
                    }
                }
            }
        }
    }

    closeRange();
    if (normals.empty()) {
        smoothNormals(vertices_, indices_);
    }
    if (materials_.empty()) {
        ensureMaterial("default");
    }
    if (vertices_.empty() || indices_.empty()) {
        error = "OBJ contains no triangle geometry: " + path;
        return false;
    }

    og::Vec3 minimum(FLT_MAX, FLT_MAX, FLT_MAX);
    og::Vec3 maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const MeshVertex& vertex : vertices_) {
        minimum.x = std::min(minimum.x, vertex.position.x);
        minimum.y = std::min(minimum.y, vertex.position.y);
        minimum.z = std::min(minimum.z, vertex.position.z);
        maximum.x = std::max(maximum.x, vertex.position.x);
        maximum.y = std::max(maximum.y, vertex.position.y);
        maximum.z = std::max(maximum.z, vertex.position.z);
    }
    const og::Vec3 center = (minimum + maximum) * 0.5f;
    const float extent = std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
    const float scale = extent > 1e-6f ? 2.4f / extent : 1.0f;
    for (MeshVertex& vertex : vertices_) {
        vertex.position = (vertex.position - center) * scale;
    }
    floorY_ = (minimum.y - center.y) * scale - 0.10f;
    boundingRadius_ = 0.0f;
    for (const MeshVertex& vertex : vertices_) {
        boundingRadius_ = std::max(boundingRadius_, og::length(vertex.position));
    }
    boundingRadius_ = std::max(boundingRadius_, 0.5f);
    return true;
}
