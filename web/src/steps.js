// The five scenarios, each pairing teaching content with the 3D staging for it:
// where the camera sits, how the medic is posed, and what the monitor should read.
//
// Vitals are deliberately written to improve as interventions land, so the
// learner sees cause and effect rather than static numbers.

export const STEPS = [
  {
    id: 'airway',
    title: 'Assess Airway',
    phase: 'Primary survey - A',
    narration:
      'Kneel at the head. Look and listen for air movement, then open the airway with a head-tilt chin-lift. Obstruction kills faster than bleeding, so nothing else matters until air is moving.',
    actions: [
      'Look, listen, and feel for no more than 10 seconds',
      'Head-tilt chin-lift; use jaw thrust if spinal injury is suspected',
      'Sweep visible obstruction only - never blind-finger sweep',
    ],
    watch: 'Snoring or gurgling means the airway is partly blocked. Silence with effort means fully blocked.',
    vitals: { hr: 118, sys: 92, dia: 58, spo2: 88 },
    camera: { position: [-2.95, 1.8, 2.45], target: [-0.6, 0.45, -0.15] },
    medic: {
      position: [-0.95, 0, -0.72],
      rotationY: 0,
      kneel: 1,
      handL: [0.0, 0.34, 0.6],
      handR: [0.24, 0.24, 0.58],
      motion: 'tilt',
    },
    patient: { headTilt: -0.36 },
    focus: [-0.8, 0.3, -0.1],
    highlight: 'airway',
  },
  {
    id: 'pressure',
    title: 'Apply Direct Pressure',
    phase: 'Primary survey - C',
    narration:
      'Find the bleed and press hard, straight down onto the wound with a gauze pad. Firm continuous pressure over the bleeding point is what stops it - not padding, and not a loose wrap.',
    actions: [
      'Expose the wound before you commit to a pressure point',
      'Press firmly with a gloved palm over gauze and hold',
      'Add layers on top - never lift the original dressing to look',
      'Escalate to a tourniquet if bleeding continues on a limb',
    ],
    watch: 'Blood soaking through means your pressure point is off, not that you need more gauze.',
    vitals: { hr: 124, sys: 86, dia: 52, spo2: 95 },
    camera: { position: [1.05, 1.95, 2.75], target: [0.25, 0.5, -0.3] },
    medic: {
      position: [0.38, 0, -0.74],
      rotationY: 0,
      kneel: 1,
      handL: [0.02, 0.4, 0.57],
      handR: [0.04, 0.31, 0.6],
      motion: 'press',
    },
    patient: { headTilt: -0.2 },
    focus: [0.38, 0.3, -0.14],
    highlight: 'wound',
  },
  {
    id: 'vitals',
    title: 'Check Vitals Monitor',
    phase: 'Reassessment',
    narration:
      'Read the monitor as a trend, not a snapshot. Rising heart rate with falling blood pressure is compensated shock - the patient is losing volume faster than you are replacing it.',
    actions: [
      'Read heart rate, blood pressure, and oxygen saturation together',
      'Compare against the last set - direction matters more than any value',
      'Confirm the trace matches a real pulse before you trust the number',
    ],
    watch: 'A narrowing gap between systolic and diastolic pressure is an early shock signal.',
    vitals: { hr: 118, sys: 88, dia: 54, spo2: 96 },
    camera: { position: [0.15, 2.05, 1.95], target: [1.35, 0.85, -0.9] },
    medic: {
      position: [1.08, 0, -1.52],
      rotationY: 0.87,
      kneel: 0,
      handL: [-0.2, 0.72, 0.08],
      handR: [0.06, 0.98, 0.52],
      motion: 'observe',
    },
    patient: { headTilt: -0.2 },
    focus: [1.74, 0.95, -0.9],
    highlight: 'monitor',
  },
  {
    id: 'iv',
    title: 'Establish IV Access',
    phase: 'Intervention',
    narration:
      'Get a large-bore cannula into the forearm and start fluids. Access is easiest before pressure drops further, so site it early rather than waiting for the patient to deteriorate.',
    actions: [
      'Choose a large-bore catheter - flow scales sharply with diameter',
      'Anchor the vein, insert bevel up at a shallow angle',
      'Watch for flashback, advance the cannula, withdraw the needle',
      'Secure the line and confirm free flow before taping down',
    ],
    watch: 'Two smaller lines beat one failed attempt at a big one. Do not chase a collapsed vein.',
    vitals: { hr: 104, sys: 104, dia: 64, spo2: 97 },
    camera: { position: [-2.15, 1.85, 2.55], target: [-0.4, 0.5, -0.4] },
    medic: {
      position: [-0.2, 0, -0.74],
      rotationY: 0,
      kneel: 1,
      handL: [-0.08, 0.3, 0.5],
      handR: [0.17, 0.29, 0.44],
      motion: 'insert',
    },
    patient: { headTilt: -0.2 },
    focus: [-0.06, 0.26, -0.3],
    highlight: 'iv',
  },
  {
    id: 'backup',
    title: 'Call for Backup',
    phase: 'Escalation',
    narration:
      'Hand over in a fixed structure so nothing is lost under pressure. State who you are, what you found, what you did, and exactly what you need - then confirm it was heard.',
    actions: [
      'Identify yourself and your location first',
      'Give mechanism, findings, and interventions already performed',
      'State the resource you need and the time frame',
      'Read back the response to confirm it was received',
    ],
    watch: 'Call early. Backup that is still ten minutes out is not backup when the patient crashes.',
    vitals: { hr: 96, sys: 112, dia: 70, spo2: 98 },
    camera: { position: [0.55, 1.95, 2.95], target: [0.05, 0.9, -0.95] },
    medic: {
      position: [0.15, 0, -1.5],
      rotationY: 0.2,
      kneel: 0,
      handL: [-0.22, 0.7, 0.06],
      handR: [0.17, 1.14, 0.12],
      motion: 'radio',
    },
    patient: { headTilt: -0.2 },
    focus: [0.15, 1.2, -1.5],
    highlight: 'radio',
  },
];
