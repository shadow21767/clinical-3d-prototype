import { useMemo, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { damp } from './rig';
import { EQUIPMENT, EQUIPMENT_LIGHT } from './materials';

const W = 640;
const H = 320;

// One cardiac cycle, phase in [0,1). Rough PQRST morphology - enough for the
// trace to read as a heartbeat rather than a sine wave.
function ecg(p) {
  if (p < 0.1) return 0;
  if (p < 0.18) return Math.sin(((p - 0.1) / 0.08) * Math.PI) * 0.12; // P
  if (p < 0.24) return 0;
  if (p < 0.27) return -((p - 0.24) / 0.03) * 0.12; // Q
  if (p < 0.31) return -0.12 + ((p - 0.27) / 0.04) * 1.12; // R up
  if (p < 0.35) return 1.0 - ((p - 0.31) / 0.04) * 1.3; // R down
  if (p < 0.39) return -0.3 + ((p - 0.35) / 0.04) * 0.3; // S recovery
  if (p < 0.52) return 0;
  if (p < 0.68) return Math.sin(((p - 0.52) / 0.16) * Math.PI) * 0.26; // T
  return 0;
}

export function VitalsMonitor({ step, steps, active }) {
  const screenRef = useRef();
  const bodyRef = useRef();

  const { canvas, ctx, texture } = useMemo(() => {
    const c = document.createElement('canvas');
    c.width = W;
    c.height = H;
    const context = c.getContext('2d');
    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    return { canvas: c, ctx: context, texture: tex };
  }, []);

  const shown = useMemo(() => ({ ...steps[0].vitals, phase: 0, glow: 0 }), [steps]);

  useFrame((_, rawDt) => {
    const dt = Math.min(rawDt, 0.05);
    const target = steps[step].vitals;

    shown.hr = damp(shown.hr, target.hr, 1.2, dt);
    shown.sys = damp(shown.sys, target.sys, 1.2, dt);
    shown.dia = damp(shown.dia, target.dia, 1.2, dt);
    shown.spo2 = damp(shown.spo2, target.spo2, 1.2, dt);
    shown.phase = (shown.phase + (dt * shown.hr) / 60) % 1;
    shown.glow = damp(shown.glow, active ? 1 : 0, 4, dt);

    if (bodyRef.current) {
      // Subtle cool lift only; a green emissive here reads as a glowing bezel.
      bodyRef.current.material.emissiveIntensity = shown.glow * 0.12;
    }

    // --- repaint the screen ---
    ctx.fillStyle = '#06131a';
    ctx.fillRect(0, 0, W, H);

    ctx.strokeStyle = 'rgba(80,140,160,0.16)';
    ctx.lineWidth = 1;
    for (let x = 0; x <= W; x += 32) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, H);
      ctx.stroke();
    }
    for (let y = 0; y <= H; y += 32) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(W, y);
      ctx.stroke();
    }

    // ECG trace across the upper two thirds, scrolling right to left.
    const traceH = 150;
    const midY = 96;
    const cycles = 3.2;
    ctx.strokeStyle = '#4dfb8b';
    ctx.lineWidth = 3;
    ctx.lineJoin = 'round';
    ctx.beginPath();
    for (let px = 0; px <= W; px++) {
      const p = ((px / W) * cycles + shown.phase) % 1;
      const y = midY - ecg(p) * traceH * 0.55;
      if (px === 0) ctx.moveTo(px, y);
      else ctx.lineTo(px, y);
    }
    ctx.stroke();

    // Plethysmograph below it.
    ctx.strokeStyle = '#5fd0ff';
    ctx.lineWidth = 2.5;
    ctx.beginPath();
    for (let px = 0; px <= W; px++) {
      const p = ((px / W) * cycles + shown.phase) % 1;
      const pulse = Math.max(0, Math.sin(p * Math.PI * 2)) ** 1.6;
      const y = 196 - pulse * 34;
      if (px === 0) ctx.moveTo(px, y);
      else ctx.lineTo(px, y);
    }
    ctx.stroke();

    // Readouts.
    ctx.textBaseline = 'alphabetic';
    const cell = (label, value, unit, color, x) => {
      ctx.fillStyle = 'rgba(160,190,205,0.75)';
      ctx.font = '600 20px ui-sans-serif, system-ui, sans-serif';
      ctx.fillText(label, x, 248);
      ctx.fillStyle = color;
      ctx.font = '700 54px ui-sans-serif, system-ui, sans-serif';
      ctx.fillText(value, x, 300);
      const w = ctx.measureText(value).width;
      ctx.fillStyle = 'rgba(160,190,205,0.7)';
      ctx.font = '600 18px ui-sans-serif, system-ui, sans-serif';
      ctx.fillText(unit, x + w + 8, 300);
    };

    cell('HR', String(Math.round(shown.hr)), 'bpm', '#4dfb8b', 24);
    cell(
      'NIBP',
      `${Math.round(shown.sys)}/${Math.round(shown.dia)}`,
      'mmHg',
      shown.sys < 100 ? '#ffb340' : '#e9eef2',
      224,
    );
    cell('SpO2', String(Math.round(shown.spo2)), '%', shown.spo2 < 92 ? '#ff6b5e' : '#5fd0ff', 470);

    if (shown.sys < 100 || shown.spo2 < 92) {
      ctx.fillStyle = 'rgba(255,120,80,0.9)';
      ctx.font = '700 18px ui-sans-serif, system-ui, sans-serif';
      ctx.fillText('CHECK PATIENT', 430, 32);
    }

    texture.needsUpdate = true;
  });

  return (
    <group position={[1.74, 0, -0.9]} rotation={[0, -0.55, 0]}>
      {/* stand */}
      <mesh position={[0, 0.02, 0]} receiveShadow>
        <cylinderGeometry args={[0.19, 0.22, 0.04, 20]} />
        <meshStandardMaterial color={EQUIPMENT} roughness={0.6} />
      </mesh>
      <mesh position={[0, 0.4, 0]} castShadow>
        <cylinderGeometry args={[0.032, 0.032, 0.78, 14]} />
        <meshStandardMaterial color={EQUIPMENT_LIGHT} roughness={0.4} metalness={0.5} />
      </mesh>

      {/* housing */}
      <mesh ref={bodyRef} position={[0, 0.98, 0]} castShadow>
        <boxGeometry args={[0.62, 0.42, 0.1]} />
        <meshStandardMaterial
          color={EQUIPMENT}
          roughness={0.5}
          emissive="#2b4a5c"
          emissiveIntensity={0}
        />
      </mesh>

      {/* screen */}
      <mesh ref={screenRef} position={[0, 0.99, 0.052]}>
        <planeGeometry args={[0.55, 0.33]} />
        <meshBasicMaterial map={texture} toneMapped={false} />
      </mesh>
    </group>
  );
}
