#include "scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// Palette, matching web/src/scene/materials.js.
const glm::vec4 SKIN{0.851f, 0.627f, 0.467f, 1.0f};
const glm::vec4 SKIN_PATIENT{0.812f, 0.608f, 0.463f, 1.0f};
const glm::vec4 SCRUBS{0.184f, 0.561f, 0.525f, 1.0f};
const glm::vec4 SCRUBS_DARK{0.137f, 0.439f, 0.416f, 1.0f};
const glm::vec4 GLOVE{0.247f, 0.498f, 0.816f, 1.0f};
const glm::vec4 GOWN{0.647f, 0.702f, 0.749f, 1.0f};
const glm::vec4 HAIR{0.227f, 0.169f, 0.137f, 1.0f};
const glm::vec4 EQUIP{0.165f, 0.184f, 0.212f, 1.0f};
const glm::vec4 EQUIP_LIGHT{0.271f, 0.302f, 0.341f, 1.0f};
const glm::vec4 BLOOD{0.639f, 0.086f, 0.114f, 1.0f};
const glm::vec4 GAUZE{0.933f, 0.945f, 0.949f, 1.0f};
const glm::vec4 FLUID{0.812f, 0.902f, 0.961f, 1.0f};
const glm::vec4 ACCENT{0.373f, 0.816f, 1.0f, 1.0f};
const glm::vec4 SIGNAL{1.0f, 0.820f, 0.400f, 1.0f};

// Medic proportions.
constexpr float UPPER_ARM = 0.26f;
constexpr float FOREARM = 0.25f;
constexpr float STAND_HIP = 0.62f, STAND_SHOULDER = 1.12f, STAND_HEAD = 1.34f, STAND_LEAN = 0.0f;
constexpr float KNEEL_HIP = 0.36f, KNEEL_SHOULDER = 0.84f, KNEEL_HEAD = 1.04f, KNEEL_LEAN = 0.06f;

// Patient landmarks. Supine along X, head at -X, medic works from -Z.
const glm::vec3 NECK{-0.66f, 0.25f, 0.0f};

const glm::vec3 IV_POLE{-1.35f, 0.0f, -1.05f};
const glm::vec3 MONITOR{1.74f, 0.0f, -0.90f};
constexpr float MONITOR_ROT = -0.55f;

// Exponential smoothing that is stable at any frame rate.
float damp(float current, float target, float lambda, float dt) {
    return glm::mix(current, target, 1.0f - std::exp(-lambda * dt));
}

glm::vec3 damp(const glm::vec3& current, const glm::vec3& target, float lambda, float dt) {
    const float t = 1.0f - std::exp(-lambda * dt);
    return glm::mix(current, target, t);
}

}  // namespace

glm::mat4 boneMatrix(const glm::vec3& a, const glm::vec3& b, float radius) {
    const glm::vec3 d = b - a;
    const float len = glm::length(d);
    if (len < 1e-5f) return glm::scale(glm::translate(glm::mat4(1.0f), a), glm::vec3(radius));

    const glm::vec3 y = d / len;
    const glm::vec3 helper = std::abs(y.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    const glm::vec3 x = glm::normalize(glm::cross(helper, y));
    const glm::vec3 z = glm::cross(x, y);

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(x * radius, 0.0f);
    m[1] = glm::vec4(y * len, 0.0f);
    m[2] = glm::vec4(z * radius, 0.0f);
    m[3] = glm::vec4(a + d * 0.5f, 1.0f);
    return m;
}

glm::mat4 jointMatrix(const glm::vec3& p, float radius) {
    return glm::scale(glm::translate(glm::mat4(1.0f), p), glm::vec3(radius));
}

glm::vec3 solveElbow(const glm::vec3& shoulder, const glm::vec3& hand, float upperLen, float lowerLen,
                     const glm::vec3& pole) {
    glm::vec3 dir = hand - shoulder;
    float reach = glm::length(dir);
    if (reach < 1e-5f) reach = 1e-5f;
    reach = std::min(reach, (upperLen + lowerLen) * 0.999f);
    dir = glm::normalize(dir);

    const float along = (upperLen * upperLen - lowerLen * lowerLen + reach * reach) / (2.0f * reach);
    const float lift = std::sqrt(std::max(0.0f, upperLen * upperLen - along * along));

    glm::vec3 perp = pole - dir * glm::dot(pole, dir);
    if (glm::dot(perp, perp) < 1e-6f) perp = glm::vec3(0, -1, 0);
    perp = glm::normalize(perp);

    return shoulder + dir * along + perp * lift;
}

Scene::Scene() {
    const Step& s = steps()[0];
    medicPos_ = s.medic.position;
    medicRotY_ = s.medic.rotationY;
    medicKneel_ = s.medic.kneel;
    handL_ = s.medic.handL;
    handR_ = s.medic.handR;
    headTilt_ = s.patientHeadTilt;
    camPos_ = s.cameraPos;
    camTarget_ = s.cameraTarget;
    shown_ = s.vitals;
}

void Scene::setCamera(const glm::vec3& pos, const glm::vec3& target) {
    camPos_ = pos;
    camTarget_ = target;
}

void Scene::update(float rawDt, int step, float time) {
    const float dt = std::min(rawDt, 0.05f);
    const Step& s = steps()[step];

    medicPos_ = damp(medicPos_, s.medic.position, 4.0f, dt);
    medicRotY_ = damp(medicRotY_, s.medic.rotationY, 4.0f, dt);
    medicKneel_ = damp(medicKneel_, s.medic.kneel, 4.0f, dt);
    handL_ = damp(handL_, s.medic.handL, 5.0f, dt);
    handR_ = damp(handR_, s.medic.handR, 5.0f, dt);
    headTilt_ = damp(headTilt_, s.patientHeadTilt, 3.0f, dt);
    bleed_ = damp(bleed_, step >= 1 ? 0.25f : 1.0f, 1.5f, dt);

    if (following_) {
        camPos_ = damp(camPos_, s.cameraPos, 2.6f, dt);
        camTarget_ = damp(camTarget_, s.cameraTarget, 2.6f, dt);
    }

    shown_.hr = damp(shown_.hr, s.vitals.hr, 1.2f, dt);
    shown_.sys = damp(shown_.sys, s.vitals.sys, 1.2f, dt);
    shown_.dia = damp(shown_.dia, s.vitals.dia, 1.2f, dt);
    shown_.spo2 = damp(shown_.spo2, s.vitals.spo2, 1.2f, dt);
    ecgPhase_ = std::fmod(ecgPhase_ + dt * shown_.hr / 60.0f, 1.0f);

    opaque_.clear();
    blended_.clear();

    // Ground and backboard. The floor is wide enough that its edge stays out of
    // frame at every step's camera.
    opaque_.push_back({MESH_BOX,
                       glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.05f, 0)),
                                  glm::vec3(70.0f, 0.1f, 70.0f)),
                       {0.129f, 0.161f, 0.204f, 1.0f}});
    opaque_.push_back({MESH_BOX,
                       glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.05f, 0.05f, 0)),
                                  glm::vec3(2.35f, 0.09f, 0.86f)),
                       {0.122f, 0.157f, 0.200f, 1.0f}});

    buildPatient(step, time);
    buildMedic(step, time);
    buildProps(step, time);
    buildHighlights(step, time);
}

void Scene::buildPatient(int step, float time) {
    const float breathe = std::sin(time * 1.9f) * 0.008f;

    // One capsule for the whole torso. Splitting it into segments leaves either
    // bright end-cap discs or visible bulges where the pieces meet.
    const float torsoR = 0.150f + breathe;
    opaque_.push_back({MESH_CYLINDER, boneMatrix({-0.60f, 0.24f, 0}, {0.20f, 0.245f, 0}, torsoR), GOWN});
    opaque_.push_back({MESH_SPHERE, jointMatrix({-0.60f, 0.24f, 0}, torsoR), GOWN});
    opaque_.push_back({MESH_SPHERE, jointMatrix({0.20f, 0.245f, 0}, torsoR), GOWN});
    opaque_.push_back({MESH_SPHERE, jointMatrix(NECK, 0.10f), SKIN_PATIENT});

    // Head, pivoting at the neck so the tilt reads as an airway maneuver.
    const glm::mat4 headPivot = glm::rotate(glm::translate(glm::mat4(1.0f), NECK), headTilt_, glm::vec3(0, 0, 1));
    opaque_.push_back({MESH_SPHERE,
                       glm::scale(glm::translate(headPivot, glm::vec3(-0.16f, 0.0f, 0.0f)), glm::vec3(0.135f)),
                       SKIN_PATIENT});
    opaque_.push_back({MESH_SPHERE,
                       glm::scale(glm::translate(headPivot, glm::vec3(-0.18f, 0.055f, 0.0f)), glm::vec3(0.125f)),
                       HAIR});

    for (int side = -1; side <= 1; side += 2) {
        const glm::vec3 shoulder{-0.50f, 0.26f, side * 0.19f};
        const glm::vec3 hand{0.14f, 0.16f, side * 0.34f};
        const glm::vec3 pole = glm::normalize(glm::vec3(0.0f, -0.9f, side * 0.5f));
        const glm::vec3 elbow = solveElbow(shoulder, hand, 0.24f, 0.24f, pole);

        opaque_.push_back({MESH_CYLINDER, boneMatrix(shoulder, elbow, 0.050f), SKIN_PATIENT});
        opaque_.push_back({MESH_SPHERE, jointMatrix(elbow, 0.048f), SKIN_PATIENT});
        opaque_.push_back({MESH_CYLINDER, boneMatrix(elbow, hand, 0.044f), SKIN_PATIENT});
        opaque_.push_back({MESH_SPHERE, jointMatrix(hand, 0.052f), SKIN_PATIENT});
    }

    for (int side = -1; side <= 1; side += 2) {
        const glm::vec3 hip{0.18f, 0.24f, side * 0.085f};
        const glm::vec3 knee{0.65f, 0.21f, side * 0.105f};
        const glm::vec3 foot{1.06f, 0.15f, side * 0.105f};
        opaque_.push_back({MESH_CYLINDER, boneMatrix(hip, knee, 0.076f), GOWN});
        opaque_.push_back({MESH_SPHERE, jointMatrix(knee, 0.070f), SKIN_PATIENT});
        opaque_.push_back({MESH_CYLINDER, boneMatrix(knee, foot, 0.058f), SKIN_PATIENT});
        opaque_.push_back({MESH_SPHERE, jointMatrix(foot, 0.066f), SKIN_PATIENT});
    }

    // Wound, then the dressing once pressure has been applied.
    glm::vec4 blood = BLOOD;
    blood.a = 0.35f + bleed_ * 0.6f;
    glm::mat4 woundM = glm::translate(glm::mat4(1.0f), {0.42f, 0.30f, -0.13f});
    woundM = glm::rotate(woundM, -0.55f, glm::vec3(1, 0, 0));
    blended_.push_back({MESH_QUAD, glm::scale(woundM, glm::vec3(0.16f * (0.7f + bleed_ * 0.5f))), blood});

    if (step >= 1) {
        glm::mat4 padM = glm::translate(glm::mat4(1.0f), {0.42f, 0.312f, -0.145f});
        padM = glm::rotate(padM, -0.55f, glm::vec3(1, 0, 0));
        glm::vec4 pad = glm::mix(GAUZE, BLOOD, (1.0f - bleed_) * 0.22f);
        opaque_.push_back({MESH_BOX, glm::scale(padM, glm::vec3(0.13f, 0.105f, 0.014f)), pad});
    }

    if (step >= 3) {
        glm::mat4 cannula = glm::translate(glm::mat4(1.0f), {0.0f, 0.20f, -0.31f});
        cannula = glm::rotate(cannula, glm::radians(90.0f), glm::vec3(0, 0, 1));
        opaque_.push_back({MESH_CYLINDER, glm::scale(cannula, glm::vec3(0.011f, 0.07f, 0.011f)),
                           {0.910f, 0.886f, 0.784f, 1.0f}});
    }
}

void Scene::buildMedic(int step, float time) {
    const Step& s = steps()[step];
    const float k = medicKneel_;
    const float hipY = glm::mix(STAND_HIP, KNEEL_HIP, k);
    const float shoulderY = glm::mix(STAND_SHOULDER, KNEEL_SHOULDER, k);
    const float headY = glm::mix(STAND_HEAD, KNEEL_HEAD, k);
    const float lean = glm::mix(STAND_LEAN, KNEEL_LEAN, k);
    const float breathe = std::sin(time * 1.6f) * 0.006f;

    // Per-scenario motion layered over the idle pose.
    float handDY = 0.0f, handDZ = 0.0f;
    switch (s.medic.motion) {
        case Motion::Press:
            handDY = -std::abs(std::sin(time * 1.5f)) * 0.022f;
            break;
        case Motion::Tilt:
            handDZ = std::sin(time * 0.9f) * 0.012f;
            break;
        case Motion::Insert:
            handDZ = std::sin(time * 0.7f) * 0.018f;
            break;
        case Motion::Observe:
            handDY = std::sin(time * 1.1f) * 0.015f;
            break;
        case Motion::Radio:
            break;
    }

    // Local space: +Z is forward. Everything is posed locally then transformed.
    const glm::mat4 root =
        glm::rotate(glm::translate(glm::mat4(1.0f), medicPos_), medicRotY_, glm::vec3(0, 1, 0));
    auto place = [&](const glm::mat4& local) { return root * local; };

    const glm::vec3 hip{0.0f, hipY, lean * 0.4f};
    const glm::vec3 chest{0.0f, shoulderY, lean};
    opaque_.push_back({MESH_CYLINDER, place(boneMatrix(hip, chest, 0.145f + breathe)), SCRUBS});
    opaque_.push_back({MESH_SPHERE, place(jointMatrix(hip, 0.130f)), SCRUBS_DARK});
    opaque_.push_back({MESH_SPHERE, place(jointMatrix(chest, 0.125f)), SCRUBS});
    opaque_.push_back({MESH_SPHERE, place(jointMatrix({0.0f, headY, lean * 1.4f}, 0.125f)), SKIN});
    opaque_.push_back({MESH_SPHERE, place(jointMatrix({0.0f, headY + 0.045f, lean * 1.4f}, 0.115f)), HAIR});

    for (int side = -1; side <= 1; side += 2) {
        const bool isLeft = side < 0;
        glm::vec3 hand = isLeft ? handL_ : handR_;
        hand.y += handDY;
        hand.z += handDZ;

        const glm::vec3 shoulder{side * 0.17f, shoulderY - 0.03f, lean};
        const glm::vec3 pole = glm::normalize(glm::vec3(side * 0.6f, -0.75f, 0.2f));
        const glm::vec3 elbow = solveElbow(shoulder, hand, UPPER_ARM, FOREARM, pole);

        opaque_.push_back({MESH_SPHERE, place(jointMatrix(shoulder, 0.075f)), SCRUBS});
        opaque_.push_back({MESH_CYLINDER, place(boneMatrix(shoulder, elbow, 0.055f)), SCRUBS});
        opaque_.push_back({MESH_SPHERE, place(jointMatrix(elbow, 0.055f)), SCRUBS});
        opaque_.push_back({MESH_CYLINDER, place(boneMatrix(elbow, hand, 0.048f)), SKIN});
        opaque_.push_back({MESH_SPHERE, place(jointMatrix(hand, 0.062f)), GLOVE});
    }

    for (int side = -1; side <= 1; side += 2) {
        const bool isLeft = side < 0;
        const glm::vec3 hip2{side * 0.095f, hipY, lean * 0.3f};

        const glm::vec3 standKnee{side * 0.10f, 0.34f, 0.02f};
        const glm::vec3 standFoot{side * 0.10f, 0.055f, 0.06f};
        const glm::vec3 kneelKnee = isLeft ? glm::vec3(-0.11f, 0.12f, 0.08f) : glm::vec3(0.13f, 0.31f, 0.22f);
        const glm::vec3 kneelFoot = isLeft ? glm::vec3(-0.11f, 0.06f, -0.26f) : glm::vec3(0.13f, 0.055f, 0.32f);

        const glm::vec3 knee = glm::mix(standKnee, kneelKnee, k);
        const glm::vec3 foot = glm::mix(standFoot, kneelFoot, k);

        opaque_.push_back({MESH_CYLINDER, place(boneMatrix(hip2, knee, 0.078f)), SCRUBS_DARK});
        opaque_.push_back({MESH_SPHERE, place(jointMatrix(knee, 0.072f)), SCRUBS_DARK});
        opaque_.push_back({MESH_CYLINDER, place(boneMatrix(knee, foot, 0.065f)), SCRUBS_DARK});
        opaque_.push_back({MESH_SPHERE, place(jointMatrix(foot, 0.070f)), EQUIP});
    }

    if (s.medic.motion == Motion::Radio) {
        glm::mat4 radio = glm::translate(glm::mat4(1.0f), handR_ + glm::vec3(0.03f, 0.02f, 0.04f));
        radio = glm::rotate(radio, -0.35f, glm::vec3(0, 0, 1));
        opaque_.push_back({MESH_BOX, place(glm::scale(radio, glm::vec3(0.05f, 0.13f, 0.035f))),
                           {0.113f, 0.129f, 0.149f, 1.0f}});
    }
}

void Scene::buildProps(int step, float time) {
    const Step& s = steps()[step];

    // ---- vitals monitor ----
    const glm::mat4 monitorRoot =
        glm::rotate(glm::translate(glm::mat4(1.0f), MONITOR), MONITOR_ROT, glm::vec3(0, 1, 0));
    opaque_.push_back({MESH_CYLINDER,
                       monitorRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.02f, 0)),
                                                glm::vec3(0.20f, 0.04f, 0.20f)),
                       EQUIP});
    opaque_.push_back({MESH_CYLINDER,
                       monitorRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.40f, 0)),
                                                glm::vec3(0.032f, 0.78f, 0.032f)),
                       EQUIP_LIGHT});
    opaque_.push_back({MESH_BOX,
                       monitorRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.98f, 0)),
                                                glm::vec3(0.62f, 0.42f, 0.10f)),
                       EQUIP});
    monitorScreen_ = monitorRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.99f, 0.052f)),
                                              glm::vec3(0.55f, 0.33f, 1.0f));

    // ---- IV pole ----
    const glm::mat4 poleRoot = glm::translate(glm::mat4(1.0f), IV_POLE);
    opaque_.push_back({MESH_CYLINDER,
                       poleRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.02f, 0)),
                                             glm::vec3(0.18f, 0.04f, 0.18f)),
                       EQUIP});
    opaque_.push_back({MESH_CYLINDER,
                       poleRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.72f, 0)),
                                             glm::vec3(0.022f, 1.42f, 0.022f)),
                       EQUIP_LIGHT});
    opaque_.push_back({MESH_BOX,
                       poleRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.02f, 1.30f, 0)),
                                             glm::vec3(0.14f, 0.22f, 0.06f)),
                       FLUID});
    opaque_.push_back({MESH_CYLINDER,
                       poleRoot * glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.02f, 1.16f, 0)),
                                             glm::vec3(0.022f, 0.09f, 0.022f)),
                       {0.875f, 0.945f, 0.984f, 0.65f}});

    const bool running = step >= 3;
    if (running) {
        // Drops falling through the chamber.
        for (int i = 0; i < 3; i++) {
            const float p = std::fmod(time * 1.1f + i / 3.0f, 1.0f);
            const float scale = 0.008f * (0.7f + std::sin(p * 3.14159f) * 0.5f);
            opaque_.push_back({MESH_SPHERE,
                               poleRoot * jointMatrix({0.02f, 1.20f - p * 0.10f, 0.0f}, scale), FLUID});
        }

        // Tubing: a Catmull-Rom spline through the control points, emitted as
        // short cylinders so it hangs and curves instead of looking like pipe.
        const glm::vec3 ctrl[5] = {{0.02f, 1.16f, 0.0f},
                                   {0.16f, 0.92f, 0.20f},
                                   {0.60f, 0.44f, 0.46f},
                                   {1.06f, 0.26f, 0.70f},
                                   {1.31f, 0.24f, 0.76f}};
        auto spline = [&](float t) {
            const float u = t * 3.0f;  // four control spans
            const int i = std::min(2, (int)u);
            const float f = u - i;
            const glm::vec3& p0 = ctrl[std::max(0, i - 1)];
            const glm::vec3& p1 = ctrl[i];
            const glm::vec3& p2 = ctrl[i + 1];
            const glm::vec3& p3 = ctrl[std::min(4, i + 2)];
            return 0.5f * ((2.0f * p1) + (-p0 + p2) * f + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * f * f +
                           (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * f * f * f);
        };
        constexpr int TUBE_SEGMENTS = 14;
        glm::vec3 previous = spline(0.0f);
        for (int i = 1; i <= TUBE_SEGMENTS; i++) {
            const glm::vec3 next = spline((float)i / TUBE_SEGMENTS);
            opaque_.push_back({MESH_CYLINDER, poleRoot * boneMatrix(previous, next, 0.0075f), FLUID});
            opaque_.push_back({MESH_SPHERE, poleRoot * jointMatrix(next, 0.0075f), FLUID});
            previous = next;
        }
    }

    (void)s;
}

void Scene::buildHighlights(int step, float time) {
    const Step& s = steps()[step];
    const float pulse = 0.9f + std::sin(time * 1.9f) * 0.08f;

    // Ground marker under whatever the step is about.
    glm::mat4 ring = glm::translate(glm::mat4(1.0f), {s.focus.x, 0.012f, s.focus.z});
    blended_.push_back({MESH_RING, glm::scale(ring, glm::vec3(0.345f * pulse, 1.0f, 0.345f * pulse)),
                        {ACCENT.r, ACCENT.g, ACCENT.b, 0.32f}});

    if (s.highlight == Highlight::Airway) {
        const float breath = (std::sin(time * 1.1f) + 1.0f) * 0.5f;
        glm::mat4 cone = glm::translate(glm::mat4(1.0f), {-1.08f, 0.32f, 0.0f});
        cone = glm::rotate(cone, glm::radians(90.0f) + 0.15f, glm::vec3(0, 0, 1));
        const float scale = 0.8f + breath * 0.35f;
        blended_.push_back({MESH_CONE, glm::scale(cone, glm::vec3(0.09f * scale, 0.32f * scale, 0.09f * scale)),
                            {0.498f, 0.890f, 1.0f, 0.12f + breath * 0.28f}});
    }

    if (s.highlight == Highlight::Radio) {
        const glm::vec3 origin = s.medic.position + glm::vec3(0.2f, 1.25f, 0.1f);
        for (int i = 0; i < 3; i++) {
            const float p = std::fmod(time * 0.65f + i / 3.0f, 1.0f);
            const float r = 0.12f + p * 0.52f;
            glm::mat4 m = glm::translate(glm::mat4(1.0f), origin);
            m = glm::rotate(m, glm::radians(-70.0f), glm::vec3(1, 0, 0));
            blended_.push_back({MESH_RING_THIN, glm::scale(m, glm::vec3(r, 1.0f, r)),
                                {SIGNAL.r, SIGNAL.g, SIGNAL.b, (1.0f - p) * 0.30f}});
        }
    }

    // Hotspot markers for every step, with the active one emphasised.
    for (size_t i = 0; i < steps().size(); i++) {
        const bool active = (int)i == step;
        const glm::vec3 p = steps()[i].hotspot;
        const glm::vec4 c = active ? ACCENT : glm::vec4(0.624f, 0.714f, 0.776f, 1.0f);
        const float size = (active ? 1.55f : 1.0f) * 0.026f * pulse;
        blended_.push_back({MESH_SPHERE, jointMatrix(p, size), {c.r, c.g, c.b, 0.95f}});
        const float ringR = (active ? 1.55f : 1.0f) * 0.055f * pulse;
        blended_.push_back({MESH_RING_THIN,
                            glm::scale(glm::translate(glm::mat4(1.0f), p), glm::vec3(ringR, 1.0f, ringR)),
                            {c.r, c.g, c.b, active ? 0.55f : 0.28f}});
    }
}
