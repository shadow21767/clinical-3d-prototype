#include "mesh.hpp"

#include <glm/gtc/constants.hpp>

namespace {
constexpr float PI = glm::pi<float>();
}

MeshLibrary::MeshLibrary() {
    begin(MESH_CYLINDER);
    addCylinder(20);
    end(MESH_CYLINDER);

    begin(MESH_SPHERE);
    addSphere(20, 14);
    end(MESH_SPHERE);

    begin(MESH_BOX);
    addBox();
    end(MESH_BOX);

    begin(MESH_QUAD);
    addQuad();
    end(MESH_QUAD);

    begin(MESH_RING);
    addRing(40, 0.72f);
    end(MESH_RING);

    begin(MESH_RING_THIN);
    addRing(48, 0.94f);
    end(MESH_RING_THIN);

    begin(MESH_CONE);
    addCone(18);
    end(MESH_CONE);
}

void MeshLibrary::begin(MeshId) { markStart_ = (uint32_t)indices_.size(); }

void MeshLibrary::end(MeshId id) {
    ranges_[id].firstIndex = markStart_;
    ranges_[id].indexCount = (uint32_t)indices_.size() - markStart_;
}

void MeshLibrary::addCylinder(int segments) {
    const uint32_t base = (uint32_t)vertices_.size();

    // Side wall: two rings of vertices with outward normals.
    for (int i = 0; i <= segments; i++) {
        const float a = (float)i / segments * 2.0f * PI;
        const glm::vec3 n(std::cos(a), 0.0f, std::sin(a));
        const float u = (float)i / segments;
        vertices_.push_back({{n.x, -0.5f, n.z}, n, {u, 1.0f}});
        vertices_.push_back({{n.x, 0.5f, n.z}, n, {u, 0.0f}});
    }
    for (int i = 0; i < segments; i++) {
        const uint32_t a = base + i * 2;
        indices_.insert(indices_.end(), {a, a + 1, a + 3, a + 3, a + 2, a});
    }

    // Caps, each with its own flat normal.
    for (int cap = 0; cap < 2; cap++) {
        const float y = cap == 0 ? 0.5f : -0.5f;
        const glm::vec3 n(0.0f, cap == 0 ? 1.0f : -1.0f, 0.0f);
        const uint32_t center = (uint32_t)vertices_.size();
        vertices_.push_back({{0.0f, y, 0.0f}, n, {0.5f, 0.5f}});
        for (int i = 0; i <= segments; i++) {
            const float a = (float)i / segments * 2.0f * PI;
            vertices_.push_back({{std::cos(a), y, std::sin(a)}, n, {0.5f, 0.5f}});
        }
        for (int i = 0; i < segments; i++) {
            if (cap == 0) {
                indices_.insert(indices_.end(), {center, center + 1 + i, center + 2 + i});
            } else {
                indices_.insert(indices_.end(), {center, center + 2 + i, center + 1 + i});
            }
        }
    }
}

void MeshLibrary::addSphere(int segments, int rings) {
    const uint32_t base = (uint32_t)vertices_.size();
    for (int y = 0; y <= rings; y++) {
        const float v = (float)y / rings;
        const float phi = v * PI;
        for (int x = 0; x <= segments; x++) {
            const float u = (float)x / segments;
            const float theta = u * 2.0f * PI;
            const glm::vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi),
                              std::sin(phi) * std::sin(theta));
            vertices_.push_back({n, n, {u, v}});
        }
    }
    for (int y = 0; y < rings; y++) {
        for (int x = 0; x < segments; x++) {
            const uint32_t a = base + y * (segments + 1) + x;
            const uint32_t b = a + segments + 1;
            indices_.insert(indices_.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
}

void MeshLibrary::addBox() {
    const glm::vec3 normals[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    for (int f = 0; f < 6; f++) {
        const glm::vec3 n = normals[f];
        // Build an orthonormal basis for the face so corners wind consistently.
        glm::vec3 up = std::abs(n.y) > 0.9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        const glm::vec3 right = glm::normalize(glm::cross(up, n));
        up = glm::cross(n, right);

        const uint32_t base = (uint32_t)vertices_.size();
        const glm::vec3 c = n * 0.5f;
        vertices_.push_back({c - right * 0.5f - up * 0.5f, n, {0, 1}});
        vertices_.push_back({c + right * 0.5f - up * 0.5f, n, {1, 1}});
        vertices_.push_back({c + right * 0.5f + up * 0.5f, n, {1, 0}});
        vertices_.push_back({c - right * 0.5f + up * 0.5f, n, {0, 0}});
        indices_.insert(indices_.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }
}

void MeshLibrary::addQuad() {
    const uint32_t base = (uint32_t)vertices_.size();
    const glm::vec3 n(0, 0, 1);
    vertices_.push_back({{-0.5f, -0.5f, 0.0f}, n, {0, 1}});
    vertices_.push_back({{0.5f, -0.5f, 0.0f}, n, {1, 1}});
    vertices_.push_back({{0.5f, 0.5f, 0.0f}, n, {1, 0}});
    vertices_.push_back({{-0.5f, 0.5f, 0.0f}, n, {0, 0}});
    indices_.insert(indices_.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
}

void MeshLibrary::addRing(int segments, float inner) {
    const uint32_t base = (uint32_t)vertices_.size();
    const glm::vec3 n(0, 1, 0);
    for (int i = 0; i <= segments; i++) {
        const float a = (float)i / segments * 2.0f * PI;
        const float c = std::cos(a), s = std::sin(a);
        vertices_.push_back({{c * inner, 0.0f, s * inner}, n, {0, 0}});
        vertices_.push_back({{c, 0.0f, s}, n, {1, 1}});
    }
    for (int i = 0; i < segments; i++) {
        const uint32_t a = base + i * 2;
        indices_.insert(indices_.end(), {a, a + 1, a + 3, a + 3, a + 2, a});
    }
}

void MeshLibrary::addCone(int segments) {
    const uint32_t base = (uint32_t)vertices_.size();
    for (int i = 0; i <= segments; i++) {
        const float a = (float)i / segments * 2.0f * PI;
        const glm::vec3 dir(std::cos(a), 0.0f, std::sin(a));
        // Slanted side normal for a unit-ish cone.
        const glm::vec3 n = glm::normalize(glm::vec3(dir.x, 0.5f, dir.z));
        vertices_.push_back({{dir.x, -0.5f, dir.z}, n, {(float)i / segments, 1.0f}});
        vertices_.push_back({{0.0f, 0.5f, 0.0f}, n, {(float)i / segments, 0.0f}});
    }
    for (int i = 0; i < segments; i++) {
        const uint32_t a = base + i * 2;
        indices_.insert(indices_.end(), {a, a + 1, a + 2});
    }
}
