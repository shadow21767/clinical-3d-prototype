// Builds the frame's draw list: poses the medic and patient, places the props,
// and eases every animated value toward the active step's staging.
#pragma once

#include "mesh.hpp"
#include "steps.hpp"

#include <glm/glm.hpp>
#include <vector>

struct DrawItem {
    MeshId mesh;
    glm::mat4 model;
    glm::vec4 color;
};

// Transform helpers shared by the rig.
glm::mat4 boneMatrix(const glm::vec3& a, const glm::vec3& b, float radius);
glm::mat4 jointMatrix(const glm::vec3& p, float radius);
glm::vec3 solveElbow(const glm::vec3& shoulder, const glm::vec3& hand, float upperLen, float lowerLen,
                     const glm::vec3& pole);

class Scene {
public:
    Scene();

    void update(float dt, int step, float time);

    const std::vector<DrawItem>& opaque() const { return opaque_; }
    const std::vector<DrawItem>& blended() const { return blended_; }

    // Where the monitor screen quad sits, for the textured draw.
    const glm::mat4& monitorScreen() const { return monitorScreen_; }

    // Camera, either eased to the step framing or driven by the orbit controls.
    glm::vec3 cameraPos() const { return camPos_; }
    glm::vec3 cameraTarget() const { return camTarget_; }
    void setCamera(const glm::vec3& pos, const glm::vec3& target);
    void setFollowing(bool v) { following_ = v; }
    bool following() const { return following_; }

    const Vitals& shownVitals() const { return shown_; }
    float ecgPhase() const { return ecgPhase_; }

private:
    void buildPatient(int step, float time);
    void buildMedic(int step, float time);
    void buildProps(int step, float time);
    void buildHighlights(int step, float time);

    std::vector<DrawItem> opaque_;
    std::vector<DrawItem> blended_;
    glm::mat4 monitorScreen_{1.0f};

    // Animated medic state.
    glm::vec3 medicPos_;
    float medicRotY_ = 0.0f;
    float medicKneel_ = 1.0f;
    glm::vec3 handL_, handR_;

    // Animated patient state.
    float headTilt_ = 0.0f;
    float bleed_ = 1.0f;

    glm::vec3 camPos_, camTarget_;
    bool following_ = true;

    Vitals shown_{};
    float ecgPhase_ = 0.0f;
};
