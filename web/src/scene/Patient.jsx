import { useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { setBone, setJoint, solveElbow, damp } from './rig';
import { SKIN_PATIENT, GOWN, HAIR, BLOOD, GAUZE } from './materials';

// Supine along the X axis: head at -X, feet at +X. The medic works from -Z.
const NECK = new THREE.Vector3(-0.66, 0.25, 0);
const HEAD_OFFSET = new THREE.Vector3(-0.16, 0.0, 0);

export function Patient({ step, steps }) {
  const parts = useRef({});
  const state = useMemo(() => ({ tilt: steps[0].patient.headTilt, bleed: 1 }), [steps]);

  const scratch = useMemo(
    () => ({
      a: new THREE.Vector3(),
      b: new THREE.Vector3(),
      elbow: new THREE.Vector3(),
      pole: new THREE.Vector3(),
      head: new THREE.Vector3(),
    }),
    [],
  );

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    const cfg = steps[step];
    const t = performance.now() / 1000;

    state.tilt = damp(state.tilt, cfg.patient.headTilt, 3, dt);
    // Bleeding is controlled once pressure has been applied.
    state.bleed = damp(state.bleed, step >= 1 ? 0.25 : 1, 1.5, dt);

    const { a, b, elbow, pole, head } = scratch;
    const breathe = Math.sin(t * 1.9) * 0.008;

    a.set(-0.6, 0.24, 0);
    b.set(0.0, 0.25, 0);
    setBone(parts.current.torso, a, b, 0.152 + breathe);
    setJoint(parts.current.shoulders, a, 0.15);

    a.set(0.0, 0.25, 0);
    b.set(0.2, 0.24, 0);
    setBone(parts.current.hips, a, b, 0.145);

    // Head pivots at the neck so the tilt reads as an airway maneuver.
    if (parts.current.headGroup) {
      parts.current.headGroup.position.copy(NECK);
      parts.current.headGroup.rotation.z = state.tilt;
    }
    setJoint(parts.current.neck, NECK, 0.1);

    // Arms out along the body; the near arm is where the IV goes.
    for (const side of [-1, 1]) {
      const isNear = side < 0;
      a.set(-0.5, 0.26, side * 0.19);
      head.set(0.14, 0.16, side * 0.34);
      pole.set(0, -0.9, side * 0.5).normalize();
      solveElbow(elbow, a, head, 0.24, 0.24, pole);

      setBone(parts.current[isNear ? 'upperArmN' : 'upperArmF'], a, elbow, 0.05);
      setJoint(parts.current[isNear ? 'elbowN' : 'elbowF'], elbow, 0.048);
      setBone(parts.current[isNear ? 'forearmN' : 'forearmF'], elbow, head, 0.044);
      setJoint(parts.current[isNear ? 'handN' : 'handF'], head, 0.052);
    }

    // Legs.
    for (const side of [-1, 1]) {
      const isNear = side < 0;
      a.set(0.18, 0.24, side * 0.085);
      b.set(0.65, 0.21, side * 0.105);
      head.set(1.06, 0.15, side * 0.105);
      setBone(parts.current[isNear ? 'thighN' : 'thighF'], a, b, 0.076);
      setJoint(parts.current[isNear ? 'kneeN' : 'kneeF'], b, 0.07);
      setBone(parts.current[isNear ? 'shinN' : 'shinF'], b, head, 0.058);
      setJoint(parts.current[isNear ? 'footN' : 'footF'], head, 0.066);
    }

    // Wound: bright and wet before pressure, dressed and dulled after.
    const blood = parts.current.blood;
    if (blood) {
      blood.scale.setScalar(0.7 + state.bleed * 0.5);
      blood.material.opacity = 0.35 + state.bleed * 0.6;
    }
    const gauzePad = parts.current.gauze;
    if (gauzePad) {
      gauzePad.visible = step >= 1;
      gauzePad.material.color.setStyle(GAUZE);
      gauzePad.material.color.lerp(new THREE.Color(BLOOD), (1 - state.bleed) * 0.22);
    }

    // Cannula and taped line appear once access is established.
    const cannula = parts.current.cannula;
    if (cannula) cannula.visible = step >= 3;
  });

  const reg = (name) => (el) => {
    parts.current[name] = el;
  };

  const Bone = ({ name, color }) => (
    <mesh ref={reg(name)} castShadow>
      <cylinderGeometry args={[1, 1, 1, 14]} />
      <meshStandardMaterial color={color} roughness={0.7} />
    </mesh>
  );

  const Joint = ({ name, color }) => (
    <mesh ref={reg(name)} castShadow>
      <sphereGeometry args={[1, 18, 14]} />
      <meshStandardMaterial color={color} roughness={0.7} />
    </mesh>
  );

  return (
    <group>
      <Bone name="torso" color={GOWN} />
      <Joint name="shoulders" color={GOWN} />
      <Bone name="hips" color={GOWN} />
      <Joint name="neck" color={SKIN_PATIENT} />

      <group ref={reg('headGroup')}>
        <mesh position={HEAD_OFFSET} castShadow>
          <sphereGeometry args={[0.135, 22, 18]} />
          <meshStandardMaterial color={SKIN_PATIENT} roughness={0.7} />
        </mesh>
        <mesh position={[HEAD_OFFSET.x - 0.02, 0.055, 0]} castShadow>
          <sphereGeometry args={[0.125, 20, 16]} />
          <meshStandardMaterial color={HAIR} roughness={0.85} />
        </mesh>
      </group>

      <Bone name="upperArmN" color={SKIN_PATIENT} />
      <Bone name="upperArmF" color={SKIN_PATIENT} />
      <Joint name="elbowN" color={SKIN_PATIENT} />
      <Joint name="elbowF" color={SKIN_PATIENT} />
      <Bone name="forearmN" color={SKIN_PATIENT} />
      <Bone name="forearmF" color={SKIN_PATIENT} />
      <Joint name="handN" color={SKIN_PATIENT} />
      <Joint name="handF" color={SKIN_PATIENT} />

      <Bone name="thighN" color={GOWN} />
      <Bone name="thighF" color={GOWN} />
      <Joint name="kneeN" color={SKIN_PATIENT} />
      <Joint name="kneeF" color={SKIN_PATIENT} />
      <Bone name="shinN" color={SKIN_PATIENT} />
      <Bone name="shinF" color={SKIN_PATIENT} />
      <Joint name="footN" color={SKIN_PATIENT} />
      <Joint name="footF" color={SKIN_PATIENT} />

      {/* Wound on the near thigh */}
      <mesh ref={reg('blood')} position={[0.42, 0.3, -0.13]} rotation={[-Math.PI / 2 + 0.55, 0, 0]}>
        <circleGeometry args={[0.07, 20]} />
        <meshStandardMaterial color={BLOOD} transparent opacity={0.95} roughness={0.3} />
      </mesh>
      <mesh ref={reg('gauze')} position={[0.42, 0.308, -0.137]} rotation={[-Math.PI / 2 + 0.55, 0, 0]}>
        <boxGeometry args={[0.13, 0.105, 0.012]} />
        <meshStandardMaterial color={GAUZE} roughness={0.9} />
      </mesh>

      {/* Cannula taped to the near forearm */}
      <mesh ref={reg('cannula')} position={[0.0, 0.2, -0.31]} rotation={[0, 0, Math.PI / 2]} visible={false}>
        <cylinderGeometry args={[0.011, 0.011, 0.07, 10]} />
        <meshStandardMaterial color="#e8e2c8" roughness={0.4} />
      </mesh>
    </group>
  );
}
