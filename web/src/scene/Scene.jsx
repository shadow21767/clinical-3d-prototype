import { useMemo, useRef } from 'react';
import { useFrame, useThree } from '@react-three/fiber';
import { OrbitControls } from '@react-three/drei';
import { XROrigin } from '@react-three/xr';
import * as THREE from 'three';
import { Hotspots } from './Hotspots';
import { Medic } from './Medic';
import { Patient } from './Patient';
import { VitalsMonitor } from './VitalsMonitor';
import { IVPole } from './IVPole';
import { FocusRing, AirwayFlow, SignalRings } from './Highlights';
import { damp } from './rig';

// Eases the camera toward the active step's framing, but only while `following`
// is true. As soon as the viewer drags, orbit controls take over and the rig
// stops fighting them until the next step change or a view reset.
function CameraRig({ step, steps, following }) {
  const controls = useThree((s) => s.controls);
  const state = useMemo(() => {
    const s = steps[0];
    return {
      pos: new THREE.Vector3(...s.camera.position),
      target: new THREE.Vector3(...s.camera.target),
    };
  }, [steps]);

  useFrame(({ camera }, rawDt) => {
    if (!following || !controls) return;
    const dt = Math.min(rawDt, 0.05);
    const cam = steps[step].camera;

    state.pos.x = damp(state.pos.x, cam.position[0], 2.6, dt);
    state.pos.y = damp(state.pos.y, cam.position[1], 2.6, dt);
    state.pos.z = damp(state.pos.z, cam.position[2], 2.6, dt);
    state.target.x = damp(state.target.x, cam.target[0], 2.6, dt);
    state.target.y = damp(state.target.y, cam.target[1], 2.6, dt);
    state.target.z = damp(state.target.z, cam.target[2], 2.6, dt);

    camera.position.copy(state.pos);
    controls.target.copy(state.target);
    controls.update();
  });

  // Keep the tween's starting point in sync with wherever the viewer left the
  // camera, so resuming guided framing doesn't jump.
  useFrame(({ camera }) => {
    if (following || !controls) return;
    state.pos.copy(camera.position);
    state.target.copy(controls.target);
  });

  return null;
}

function Ground() {
  return (
    <>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, 0]} receiveShadow>
        <planeGeometry args={[26, 26]} />
        <meshStandardMaterial color="#151b22" roughness={0.95} />
      </mesh>
      {/* Backboard the patient is lying on. */}
      <mesh position={[0.05, 0.05, 0]} receiveShadow castShadow>
        <boxGeometry args={[2.35, 0.09, 0.86]} />
        <meshStandardMaterial color="#1f2833" roughness={0.8} />
      </mesh>
      <gridHelper args={[26, 52, '#243040', '#1b232d']} position={[0, 0.002, 0]} />
    </>
  );
}

export function Scene({ step, steps, following, onTakeOver, onSelect, showHotspots }) {
  const hoveringHotspot = useRef(false);
  const medicRadio = useRef(new THREE.Vector3());
  const cfg = steps[step];
  medicRadio.current.set(cfg.medic.position[0] + 0.2, 1.25, cfg.medic.position[2] + 0.1);

  return (
    <>
      <OrbitControls
        makeDefault
        // Orbit fires on any canvas press, so ignore presses that land on a
        // hotspot - otherwise selecting a marker also drops out of guided view.
        onStart={() => {
          if (!hoveringHotspot.current) onTakeOver();
        }}
        enableDamping
        dampingFactor={0.08}
        minDistance={0.5}
        maxDistance={11}
        maxPolarAngle={Math.PI / 2 - 0.04}
        target={steps[0].camera.target}
      />
      <CameraRig step={step} steps={steps} following={following} />

      {/* Where a headset wearer stands: beside the patient, facing the scene. */}
      <XROrigin position={[0.1, 0, 1.85]} />

      <ambientLight intensity={0.42} />
      <hemisphereLight args={['#9fc4e0', '#1a222b', 0.45]} />
      <directionalLight
        position={[3.2, 5.4, 3.6]}
        intensity={1.5}
        castShadow
        shadow-mapSize={[2048, 2048]}
        shadow-camera-left={-5}
        shadow-camera-right={5}
        shadow-camera-top={5}
        shadow-camera-bottom={-5}
      />
      <directionalLight position={[-3.5, 2.4, -2.5]} intensity={0.35} color="#7fb4e8" />
      <pointLight position={[0, 1.6, 1.4]} intensity={0.5} distance={7} />

      <Ground />
      <Patient step={step} steps={steps} />
      <Medic step={step} steps={steps} />
      <VitalsMonitor step={step} steps={steps} active={cfg.highlight === 'monitor'} />
      <IVPole step={step} active={cfg.highlight === 'iv'} />

      <Hotspots
        step={step}
        onSelect={onSelect}
        visible={showHotspots}
        onHoverChange={(v) => {
          hoveringHotspot.current = v;
        }}
      />
      <FocusRing step={step} steps={steps} />
      <AirwayFlow visible={cfg.highlight === 'airway'} />
      <SignalRings visible={cfg.highlight === 'radio'} position={medicRadio.current.toArray()} />

      <fog attach="fog" args={['#0b1016', 7, 20]} />
    </>
  );
}
