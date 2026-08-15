// The five scenarios: teaching content plus the staging for each one. This is
// the C++ counterpart of web/src/steps.js and the two should stay in sync.
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class Motion { Tilt, Press, Observe, Insert, Radio };

enum class Highlight { Airway, Wound, Monitor, IV, Radio };

struct Vitals {
    float hr, sys, dia, spo2;
};

struct MedicPose {
    glm::vec3 position;
    float rotationY;
    float kneel;  // 0 standing, 1 kneeling
    glm::vec3 handL;
    glm::vec3 handR;
    Motion motion;
};

struct Step {
    std::string id;
    std::string title;
    std::string phase;
    std::string narration;
    std::vector<std::string> actions;
    std::string watch;
    Vitals vitals;
    glm::vec3 cameraPos;
    glm::vec3 cameraTarget;
    MedicPose medic;
    float patientHeadTilt;
    glm::vec3 focus;
    Highlight highlight;
    glm::vec3 hotspot;
    std::string hotspotLabel;
};

const std::vector<Step>& steps();
