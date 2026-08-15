import { useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { setBone, setJoint, solveElbow, damp, dampVec } from './rig';
import { SKIN, SCRUBS, SCRUBS_DARK, GLOVE, HAIR, EQUIPMENT } from './materials';

const UPPER_ARM = 0.26;
const FOREARM = 0.25;
const THIGH = 0.28;
const SHIN = 0.28;

// Hip / shoulder / head heights for the two poses we interpolate between.
const STAND = { hip: 0.62, shoulder: 1.12, head: 1.34, lean: 0 };
const KNEEL = { hip: 0.36, shoulder: 0.84, head: 1.04, lean: 0.06 };

export function Medic({ step, steps }) {
  const parts = useRef({});
  const group = useRef();

  // Animated state, mutated in place each frame.
  const state = useMemo(
    () => ({
      pos: new THREE.Vector3(...steps[0].medic.position),
      rotY: steps[0].medic.rotationY,
      kneel: steps[0].medic.kneel,
      handL: new THREE.Vector3(...steps[0].medic.handL),
      handR: new THREE.Vector3(...steps[0].medic.handR),
    }),
    [steps],
  );

  const scratch = useMemo(
    () => ({
      a: new THREE.Vector3(),
      b: new THREE.Vector3(),
      elbow: new THREE.Vector3(),
      knee: new THREE.Vector3(),
      pole: new THREE.Vector3(),
      hand: new THREE.Vector3(),
    }),
    [],
  );

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    const cfg = steps[step].medic;
    const t = performance.now() / 1000;

    dampVec(state.pos, cfg.position, 4, dt);
    state.rotY = damp(state.rotY, cfg.rotationY, 4, dt);
    state.kneel = damp(state.kneel, cfg.kneel, 4, dt);
    dampVec(state.handL, cfg.handL, 5, dt);
    dampVec(state.handR, cfg.handR, 5, dt);

    if (group.current) {
      group.current.position.copy(state.pos);
      group.current.rotation.y = state.rotY;
    }

    const k = state.kneel;
    const hipY = THREE.MathUtils.lerp(STAND.hip, KNEEL.hip, k);
    const shoulderY = THREE.MathUtils.lerp(STAND.shoulder, KNEEL.shoulder, k);
    const headY = THREE.MathUtils.lerp(STAND.head, KNEEL.head, k);
    const lean = THREE.MathUtils.lerp(STAND.lean, KNEEL.lean, k);

    // Idle breathing, plus per-scenario motion layered on top.
    const breathe = Math.sin(t * 1.6) * 0.006;
    let handOffsetY = 0;
    let handOffsetZ = 0;
    if (cfg.motion === 'press') {
      // Compressions: a firm downward push with a slow recovery.
      handOffsetY = -Math.abs(Math.sin(t * 1.5)) * 0.022;
    } else if (cfg.motion === 'tilt') {
      handOffsetZ = Math.sin(t * 0.9) * 0.012;
    } else if (cfg.motion === 'insert') {
      // Slow, deliberate advance of the cannula.
      handOffsetZ = Math.sin(t * 0.7) * 0.018;
    } else if (cfg.motion === 'observe') {
      handOffsetY = Math.sin(t * 1.1) * 0.015;
    }

    const { a, b, elbow, knee, pole, hand } = scratch;

    // Spine and head.
    a.set(0, hipY, lean * 0.4);
    b.set(0, shoulderY, lean);
    setBone(parts.current.torso, a, b, 0.145 + breathe);
    setJoint(parts.current.hips, a, 0.13);
    setJoint(parts.current.chest, b, 0.125);

    b.set(0, headY, lean * 1.4);
    setJoint(parts.current.head, b, 0.125);
    setJoint(parts.current.hair, b.clone().setY(headY + 0.045), 0.115);

    // Arms.
    for (const side of [-1, 1]) {
      const isLeft = side < 0;
      hand.copy(isLeft ? state.handL : state.handR);
      hand.y += handOffsetY;
      hand.z += handOffsetZ;

      a.set(side * 0.17, shoulderY - 0.03, lean);
      pole.set(side * 0.6, -0.75, 0.2).normalize();
      solveElbow(elbow, a, hand, UPPER_ARM, FOREARM, pole);

      setJoint(parts.current[isLeft ? 'shoulderL' : 'shoulderR'], a, 0.075);
      setBone(parts.current[isLeft ? 'upperArmL' : 'upperArmR'], a, elbow, 0.055);
      setJoint(parts.current[isLeft ? 'elbowL' : 'elbowR'], elbow, 0.055);
      setBone(parts.current[isLeft ? 'forearmL' : 'forearmR'], elbow, hand, 0.048);
      setJoint(parts.current[isLeft ? 'handL' : 'handR'], hand, 0.062);
    }

    // Legs: standing is symmetric, kneeling drops the left knee and plants the right foot.
    for (const side of [-1, 1]) {
      const isLeft = side < 0;
      a.set(side * 0.095, hipY, lean * 0.3);

      const standKnee = [side * 0.1, 0.34, 0.02];
      const standFoot = [side * 0.1, 0.055, 0.06];
      const kneelKnee = isLeft ? [-0.11, 0.12, 0.08] : [0.13, 0.31, 0.22];
      const kneelFoot = isLeft ? [-0.11, 0.06, -0.26] : [0.13, 0.055, 0.32];

      knee.set(
        THREE.MathUtils.lerp(standKnee[0], kneelKnee[0], k),
        THREE.MathUtils.lerp(standKnee[1], kneelKnee[1], k),
        THREE.MathUtils.lerp(standKnee[2], kneelKnee[2], k),
      );
      b.set(
        THREE.MathUtils.lerp(standFoot[0], kneelFoot[0], k),
        THREE.MathUtils.lerp(standFoot[1], kneelFoot[1], k),
        THREE.MathUtils.lerp(standFoot[2], kneelFoot[2], k),
      );

      setBone(parts.current[isLeft ? 'thighL' : 'thighR'], a, knee, 0.078);
      setJoint(parts.current[isLeft ? 'kneeL' : 'kneeR'], knee, 0.072);
      setBone(parts.current[isLeft ? 'shinL' : 'shinR'], knee, b, 0.065);
      setJoint(parts.current[isLeft ? 'footL' : 'footR'], b, 0.07);
    }

    // The radio only exists while the medic is calling it in.
    const radio = parts.current.radio;
    if (radio) {
      radio.visible = cfg.motion === 'radio';
      if (radio.visible) {
        radio.position.copy(state.handR).add(new THREE.Vector3(0.03, 0.02, 0.04));
        radio.rotation.set(0.2, 0, -0.35);
      }
    }
  });

  const reg = (name) => (el) => {
    parts.current[name] = el;
  };

  const Bone = ({ name, color }) => (
    <mesh ref={reg(name)} castShadow>
      <cylinderGeometry args={[1, 1, 1, 14]} />
      <meshStandardMaterial color={color} roughness={0.65} />
    </mesh>
  );

  const Joint = ({ name, color }) => (
    <mesh ref={reg(name)} castShadow>
      <sphereGeometry args={[1, 18, 14]} />
      <meshStandardMaterial color={color} roughness={0.65} />
    </mesh>
  );

  return (
    <group ref={group}>
      <Bone name="torso" color={SCRUBS} />
      <Joint name="hips" color={SCRUBS_DARK} />
      <Joint name="chest" color={SCRUBS} />
      <Joint name="head" color={SKIN} />
      <Joint name="hair" color={HAIR} />

      <Joint name="shoulderL" color={SCRUBS} />
      <Joint name="shoulderR" color={SCRUBS} />
      <Bone name="upperArmL" color={SCRUBS} />
      <Bone name="upperArmR" color={SCRUBS} />
      <Joint name="elbowL" color={SCRUBS} />
      <Joint name="elbowR" color={SCRUBS} />
      <Bone name="forearmL" color={SKIN} />
      <Bone name="forearmR" color={SKIN} />
      <Joint name="handL" color={GLOVE} />
      <Joint name="handR" color={GLOVE} />

      <Bone name="thighL" color={SCRUBS_DARK} />
      <Bone name="thighR" color={SCRUBS_DARK} />
      <Joint name="kneeL" color={SCRUBS_DARK} />
      <Joint name="kneeR" color={SCRUBS_DARK} />
      <Bone name="shinL" color={SCRUBS_DARK} />
      <Bone name="shinR" color={SCRUBS_DARK} />
      <Joint name="footL" color={EQUIPMENT} />
      <Joint name="footR" color={EQUIPMENT} />

      <mesh ref={reg('radio')} castShadow visible={false}>
        <boxGeometry args={[0.05, 0.13, 0.035]} />
        <meshStandardMaterial color="#1d2126" roughness={0.5} />
      </mesh>
    </group>
  );
}
