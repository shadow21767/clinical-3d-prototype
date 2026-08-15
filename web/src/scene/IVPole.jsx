import { useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { damp } from './rig';
import { EQUIPMENT, EQUIPMENT_LIGHT, FLUID } from './materials';

const POLE = [-1.35, 0, -1.05];
const DROPS = 3;

// Tubing from the drip chamber down to the patient's near forearm.
function tubingCurve() {
  return new THREE.CatmullRomCurve3([
    new THREE.Vector3(0.02, 1.16, 0),
    new THREE.Vector3(0.16, 0.95, 0.18),
    new THREE.Vector3(0.62, 0.5, 0.42),
    new THREE.Vector3(1.05, 0.3, 0.68),
    new THREE.Vector3(1.31, 0.27, 0.75),
  ]);
}

export function IVPole({ step, active }) {
  const drops = useRef([]);
  const bagRef = useRef();
  const tubeRef = useRef();
  const state = useMemo(() => ({ glow: 0, flow: 0 }), []);

  const curve = useMemo(() => tubingCurve(), []);
  const running = step >= 3;

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    const t = performance.now() / 1000;

    state.glow = damp(state.glow, active ? 1 : 0, 4, dt);
    state.flow = damp(state.flow, running ? 1 : 0, 2, dt);

    if (bagRef.current) bagRef.current.material.emissiveIntensity = state.glow * 0.4;
    if (tubeRef.current) {
      tubeRef.current.visible = state.flow > 0.02;
      tubeRef.current.material.opacity = state.flow * 0.85;
    }

    // Drops fall through the chamber only once fluids are running.
    drops.current.forEach((drop, i) => {
      if (!drop) return;
      drop.visible = state.flow > 0.3;
      const p = ((t * 1.1 + i / DROPS) % 1);
      drop.position.y = 1.2 - p * 0.1;
      drop.scale.setScalar(0.7 + Math.sin(p * Math.PI) * 0.5);
    });
  });

  return (
    <group position={POLE}>
      <mesh position={[0, 0.02, 0]} receiveShadow>
        <cylinderGeometry args={[0.16, 0.19, 0.04, 18]} />
        <meshStandardMaterial color={EQUIPMENT} roughness={0.6} />
      </mesh>
      <mesh position={[0, 0.72, 0]} castShadow>
        <cylinderGeometry args={[0.022, 0.022, 1.42, 12]} />
        <meshStandardMaterial color={EQUIPMENT_LIGHT} roughness={0.35} metalness={0.6} />
      </mesh>
      <mesh position={[0.02, 1.43, 0]} rotation={[0, 0, Math.PI / 2]}>
        <cylinderGeometry args={[0.014, 0.014, 0.12, 10]} />
        <meshStandardMaterial color={EQUIPMENT_LIGHT} metalness={0.6} roughness={0.35} />
      </mesh>

      {/* fluid bag */}
      <mesh ref={bagRef} position={[0.02, 1.3, 0]} castShadow>
        <boxGeometry args={[0.14, 0.22, 0.06]} />
        <meshStandardMaterial
          color={FLUID}
          transparent
          opacity={0.92}
          roughness={0.25}
          emissive="#8fd4f5"
          emissiveIntensity={0}
        />
      </mesh>

      {/* drip chamber */}
      <mesh position={[0.02, 1.16, 0]}>
        <cylinderGeometry args={[0.022, 0.022, 0.09, 12]} />
        <meshStandardMaterial color="#dff1fb" transparent opacity={0.45} roughness={0.15} />
      </mesh>
      {Array.from({ length: DROPS }).map((_, i) => (
        <mesh
          key={i}
          ref={(el) => {
            drops.current[i] = el;
          }}
          position={[0.02, 1.2, 0]}
          visible={false}
        >
          <sphereGeometry args={[0.008, 8, 6]} />
          <meshStandardMaterial color={FLUID} />
        </mesh>
      ))}

      <mesh ref={tubeRef} visible={false}>
        <tubeGeometry args={[curve, 48, 0.008, 8, false]} />
        <meshStandardMaterial color={FLUID} transparent opacity={0} roughness={0.3} />
      </mesh>
    </group>
  );
}
