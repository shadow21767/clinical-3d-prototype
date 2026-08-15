// Procedural geometry. Every shape in the scene is one of a handful of unit
// primitives, transformed per draw, so the whole scene ships in one vertex and
// one index buffer.
#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// A contiguous slice of the shared index buffer.
struct MeshRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

enum MeshId {
    MESH_CYLINDER,  // radius 1, height 1, centered on origin, axis +Y
    MESH_SPHERE,    // radius 1
    MESH_BOX,       // 1x1x1 centered
    MESH_QUAD,      // 1x1 in XY, facing +Z
    MESH_RING,      // annulus in XZ, outer radius 1, inner 0.72
    MESH_RING_THIN, // same, but a hairline band for signal waves
    MESH_CONE,      // base radius 1 at y=-0.5, apex at y=+0.5
    MESH_COUNT,
};

// Builds all primitives into shared buffers and records where each one lives.
class MeshLibrary {
public:
    MeshLibrary();

    const std::vector<Vertex>& vertices() const { return vertices_; }
    const std::vector<uint32_t>& indices() const { return indices_; }
    const MeshRange& range(MeshId id) const { return ranges_[id]; }

private:
    void addCylinder(int segments);
    void addSphere(int segments, int rings);
    void addBox();
    void addQuad();
    void addRing(int segments, float inner);
    void addCone(int segments);

    void begin(MeshId id);
    void end(MeshId id);

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    MeshRange ranges_[MESH_COUNT];
    uint32_t markStart_ = 0;
};
