import { useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { damp } from './rig';

// Ground marker that slides to whatever the current step is about.
export function FocusRing({ step, steps }) {
  const ref = useRef();
  const pos = useMemo(() => new THREE.Vector3(...steps[0].focus), [steps]);

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    const target = steps[step].focus;
    pos.x = damp(pos.x, target[0], 4, dt);
    pos.z = damp(pos.z, target[2], 4, dt);
    if (!ref.current) return;
    ref.current.position.set(pos.x, 0.012, pos.z);
    const pulse = 0.9 + Math.sin(performance.now() / 520) * 0.08;
    ref.current.scale.setScalar(pulse);
    ref.current.material.opacity = 0.3 + Math.sin(performance.now() / 520) * 0.08;
  });

  return (
    <mesh ref={ref} rotation={[-Math.PI / 2, 0, 0]}>
      <ringGeometry args={[0.3, 0.345, 48]} />
      <meshBasicMaterial color="#5fd0ff" transparent opacity={0.3} side={THREE.DoubleSide} />
    </mesh>
  );
}

// Cone of moving air out of the mouth once the airway is opened.
export function AirwayFlow({ visible }) {
  const ref = useRef();
  const state = useMemo(() => ({ v: 0 }), []);

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    state.v = damp(state.v, visible ? 1 : 0, 5, dt);
    if (!ref.current) return;
    ref.current.visible = state.v > 0.02;
    const breath = (Math.sin(performance.now() / 900) + 1) / 2;
    ref.current.material.opacity = state.v * (0.12 + breath * 0.3);
    ref.current.scale.setScalar(0.8 + breath * 0.35);
  });

  return (
    <mesh ref={ref} position={[-1.08, 0.32, 0]} rotation={[0, 0, Math.PI / 2 + 0.15]} visible={false}>
      <coneGeometry args={[0.09, 0.32, 16, 1, true]} />
      <meshBasicMaterial color="#7fe3ff" transparent opacity={0} side={THREE.DoubleSide} />
    </mesh>
  );
}

// Expanding rings while the medic is on the radio.
export function SignalRings({ visible, position }) {
  const refs = useRef([]);
  const state = useMemo(() => ({ v: 0 }), []);

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    state.v = damp(state.v, visible ? 1 : 0, 5, dt);
    const t = performance.now() / 1000;

    refs.current.forEach((ring, i) => {
      if (!ring) return;
      ring.visible = state.v > 0.02;
      const p = (t * 0.65 + i / refs.current.length) % 1;
      ring.scale.setScalar(0.15 + p * 1.5);
      ring.material.opacity = state.v * (1 - p) * 0.5;
    });
  });

  return (
    <group position={position} rotation={[-Math.PI / 2.6, 0, 0]}>
      {[0, 1, 2].map((i) => (
        <mesh
          key={i}
          ref={(el) => {
            refs.current[i] = el;
          }}
          visible={false}
        >
          <ringGeometry args={[0.28, 0.32, 40]} />
          <meshBasicMaterial color="#ffd166" transparent opacity={0} side={THREE.DoubleSide} />
        </mesh>
      ))}
    </group>
  );
}
