# Clinical Procedure Walkthrough (web)

An interactive 3D walkthrough of a trauma primary survey. Click through five
steps and watch an animated responder carry out each one, with teaching notes
and a live vitals monitor that responds to the interventions.

## Run it

```bash
cd web
npm install
npm run dev
```

Then open http://localhost:5173.

## Interacting

| Input | Effect |
|---|---|
| Drag | Orbit the camera; this switches to Free look |
| Scroll / pinch | Zoom |
| Right-drag | Pan |
| Click a marker in the scene | Jump to that scenario |
| Arrow keys, space | Next / previous step |
| `R` | Reset back to the guided camera |
| `H` | Toggle the hotspot markers |
| Enter VR / AR | Immersive session, if the browser and hardware support it |

The camera has two modes. In **Guided view** it eases to a framing chosen per
step. The moment you drag, it hands control over to you (**Free look**) and
stops fighting your input; changing step or pressing Reset resumes guided
framing from wherever you left the camera.

### VR

The scene is modelled at roughly life size, and `XROrigin` in
`src/scene/Scene.jsx` places a headset wearer standing beside the patient. The
Enter VR button only appears when `navigator.xr` reports an immersive session is
supported, so it stays hidden in a desktop browser without a headset. WebXR
also requires a secure context - `localhost` counts, but serving to a headset
over the network needs HTTPS.

## How it fits together

| File | Role |
|---|---|
| `src/steps.js` | The five scenarios: teaching copy, camera framing, medic pose, target vitals |
| `src/scene/Scene.jsx` | Assembles the scene, lighting, and the scripted camera |
| `src/scene/Medic.jsx` | Posable responder rig, driven by two-bone IK |
| `src/scene/Patient.jsx` | Supine patient, breathing, head tilt, wound and cannula state |
| `src/scene/VitalsMonitor.jsx` | Monitor whose screen is a live 2D canvas texture (ECG, pleth, readouts) |
| `src/scene/IVPole.jsx` | Fluid bag, drip chamber, and tubing routed to the patient's forearm |
| `src/scene/Highlights.jsx` | Focus ring, airway flow cone, radio signal rings |
| `src/scene/Hotspots.jsx` | Clickable in-scene markers that jump between scenarios |
| `src/scene/rig.js` | Bone stretching, two-bone IK solver, damping helpers |

Everything is drawn from primitives, so there are no model or font downloads.
Adding or editing a scenario means editing `src/steps.js` - the scene reads its
staging from that array.

## Editing a scenario

Each entry pairs content with staging:

```js
{
  title: 'Apply Direct Pressure',
  narration: '...',           // paragraph in the side panel
  actions: ['...'],           // bulleted key actions
  watch: '...',               // the amber callout
  vitals: { hr, sys, dia, spo2 },   // monitor eases toward these
  camera: { position, target },      // where the shot sits
  medic: { position, rotationY, kneel, handL, handR, motion },
  focus: [x, y, z],           // ground marker position
}
```

`medic.kneel` is 0 for standing and 1 for kneeling; the rig interpolates. Hand
positions are in the medic's local space, where +Z is forward. `motion` selects
the idle overlay: `press`, `tilt`, `insert`, `observe`, or `radio`.

## Screenshot helper

`scripts/shoot.mjs` captures every step through a headless Chrome for checking
staging without clicking through by hand. It attaches to an existing browser:

```bash
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  --headless=new --user-data-dir=/tmp/cwprofile \
  --enable-unsafe-swiftshader --remote-debugging-port=9222 about:blank &

node scripts/shoot.mjs /tmp/cw
```

`scripts/interact.mjs` drives the interactive controls against the same browser
and prints what the UI did, optionally clicking a pixel coordinate to test a
hotspot:

```bash
node scripts/interact.mjs /tmp/cw 984 108
```

## Relation to the Vulkan prototype

The C++/Vulkan program in the parent directory is the original version, which
rendered the same five steps as colored cubes. It still builds and runs; see the
top-level README. This web app replaces it as the teaching tool because rigged
animation, lighting, and text are solved problems in a 3D framework and weeks of
work in raw Vulkan.
