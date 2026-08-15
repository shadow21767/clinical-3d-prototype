#include "steps.hpp"

const std::vector<Step>& steps() {
    static const std::vector<Step> kSteps = {
        {
            "airway",
            "Assess Airway",
            "Primary survey - A",
            "Kneel at the head. Look and listen for air movement, then open the airway with a "
            "head-tilt chin-lift. Obstruction kills faster than bleeding, so nothing else matters "
            "until air is moving.",
            {
                "Look, listen, and feel for no more than 10 seconds",
                "Head-tilt chin-lift; jaw thrust if spinal injury suspected",
                "Sweep visible obstruction only - never blind-finger sweep",
            },
            "Snoring or gurgling means the airway is partly blocked. Silence with effort means "
            "fully blocked.",
            {118.0f, 92.0f, 58.0f, 88.0f},
            {-2.95f, 1.80f, 2.45f},
            {-0.60f, 0.45f, -0.15f},
            {{-0.95f, 0.0f, -0.72f}, 0.0f, 1.0f, {0.0f, 0.34f, 0.60f}, {0.24f, 0.24f, 0.58f}, Motion::Tilt},
            -0.36f,
            {-0.80f, 0.30f, -0.10f},
            Highlight::Airway,
            {-0.95f, 0.46f, -0.12f},
            "Airway",
        },
        {
            "pressure",
            "Apply Direct Pressure",
            "Primary survey - C",
            "Find the bleed and press hard, straight down onto the wound with a gauze pad. Firm "
            "continuous pressure over the bleeding point is what stops it - not padding, and not a "
            "loose wrap.",
            {
                "Expose the wound before committing to a pressure point",
                "Press firmly with a gloved palm over gauze and hold",
                "Add layers on top - never lift the original dressing",
                "Escalate to a tourniquet if a limb keeps bleeding",
            },
            "Blood soaking through means your pressure point is off, not that you need more gauze.",
            {124.0f, 86.0f, 52.0f, 95.0f},
            {1.15f, 2.25f, 3.30f},
            {0.30f, 0.42f, -0.25f},
            {{0.38f, 0.0f, -0.74f}, 0.0f, 1.0f, {0.02f, 0.40f, 0.57f}, {0.04f, 0.31f, 0.60f}, Motion::Press},
            -0.20f,
            {0.38f, 0.30f, -0.14f},
            Highlight::Wound,
            {0.42f, 0.48f, -0.16f},
            "Bleeding wound",
        },
        {
            "vitals",
            "Check Vitals Monitor",
            "Reassessment",
            "Read the monitor as a trend, not a snapshot. Rising heart rate with falling blood "
            "pressure is compensated shock - the patient is losing volume faster than you are "
            "replacing it.",
            {
                "Read heart rate, blood pressure, and saturation together",
                "Compare against the last set - direction beats any value",
                "Confirm the trace matches a real pulse before trusting it",
            },
            "A narrowing gap between systolic and diastolic pressure is an early shock signal.",
            {118.0f, 88.0f, 54.0f, 96.0f},
            {0.15f, 2.05f, 1.95f},
            {1.35f, 0.85f, -0.90f},
            {{1.08f, 0.0f, -1.52f}, 0.87f, 0.0f, {-0.20f, 0.72f, 0.08f}, {0.06f, 0.98f, 0.52f}, Motion::Observe},
            -0.20f,
            {1.74f, 0.95f, -0.90f},
            Highlight::Monitor,
            {1.74f, 1.32f, -0.90f},
            "Vitals monitor",
        },
        {
            "iv",
            "Establish IV Access",
            "Intervention",
            "Get a large-bore cannula into the forearm and start fluids. Access is easiest before "
            "pressure drops further, so site it early rather than waiting for the patient to "
            "deteriorate.",
            {
                "Choose a large-bore catheter - flow scales with diameter",
                "Anchor the vein, insert bevel up at a shallow angle",
                "Watch for flashback, advance, then withdraw the needle",
                "Secure the line and confirm free flow before taping",
            },
            "Two smaller lines beat one failed attempt at a big one. Do not chase a collapsed vein.",
            {104.0f, 104.0f, 64.0f, 97.0f},
            {-2.15f, 1.85f, 2.55f},
            {-0.40f, 0.50f, -0.40f},
            {{-0.20f, 0.0f, -0.74f}, 0.0f, 1.0f, {-0.08f, 0.30f, 0.50f}, {0.17f, 0.29f, 0.44f}, Motion::Insert},
            -0.20f,
            {-0.06f, 0.26f, -0.30f},
            Highlight::IV,
            {-1.33f, 1.52f, -1.05f},
            "IV fluids",
        },
        {
            "backup",
            "Call for Backup",
            "Escalation",
            "Hand over in a fixed structure so nothing is lost under pressure. State who you are, "
            "what you found, what you did, and exactly what you need - then confirm it was heard.",
            {
                "Identify yourself and your location first",
                "Give mechanism, findings, and interventions performed",
                "State the resource you need and the time frame",
                "Read back the response to confirm it was received",
            },
            "Call early. Backup that is still ten minutes out is not backup when the patient "
            "crashes.",
            {96.0f, 112.0f, 70.0f, 98.0f},
            {0.55f, 1.95f, 2.95f},
            {0.05f, 0.90f, -0.95f},
            {{0.15f, 0.0f, -1.50f}, 0.20f, 0.0f, {-0.22f, 0.70f, 0.06f}, {0.17f, 1.14f, 0.12f}, Motion::Radio},
            -0.20f,
            {0.15f, 1.20f, -1.50f},
            Highlight::Radio,
            {0.15f, 1.34f, -1.50f},
            "Radio",
        },
    };
    return kSteps;
}
