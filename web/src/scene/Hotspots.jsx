import { useRef, useState } from 'react';
import { useFrame } from '@react-three/fiber';
import { Html } from '@react-three/drei';
import { useXR } from '@react-three/xr';

// Clickable points of interest. Each one jumps the walkthrough to its step, so
// the scene itself becomes navigable rather than only the side rail.
const HOTSPOTS = [
  { step: 0, position: [-0.95, 0.46, -0.12], label: 'Airway' },
  { step: 1, position: [0.42, 0.48, -0.16], label: 'Bleeding wound' },
  { step: 2, position: [1.74, 1.32, -0.9], label: 'Vitals monitor' },
  { step: 3, position: [-1.33, 1.52, -1.05], label: 'IV fluids' },
  { step: 4, position: [0.15, 1.34, -1.5], label: 'Radio' },
];

function Hotspot({ data, isActive, onSelect, labelsVisible, onHoverChange }) {
  const ref = useRef();
  const ringRef = useRef();
  const [hovered, setHovered] = useState(false);

  useFrame(() => {
    const t = performance.now() / 1000;
    const pulse = 1 + Math.sin(t * 2.4) * 0.12;
    const emphasis = isActive || hovered ? 1.55 : 1;
    if (ref.current) ref.current.scale.setScalar(0.026 * emphasis * pulse);
    if (ringRef.current) {
      ringRef.current.scale.setScalar(0.055 * emphasis * (1 + Math.sin(t * 2.4) * 0.16));
      ringRef.current.material.opacity = (isActive ? 0.55 : 0.28) * (hovered ? 1.4 : 1);
    }
  });

  const color = isActive ? '#5fd0ff' : '#9fb6c6';

  return (
    <group
      position={data.position}
      onClick={(e) => {
        e.stopPropagation();
        onSelect(data.step);
      }}
      onPointerOver={(e) => {
        e.stopPropagation();
        setHovered(true);
        onHoverChange(true);
        document.body.style.cursor = 'pointer';
      }}
      onPointerOut={() => {
        setHovered(false);
        onHoverChange(false);
        document.body.style.cursor = 'auto';
      }}
    >
      {/* Generous target so the marker is easy to hit. It has to stay `visible`
          because the raycaster skips hidden objects, so it draws nothing
          instead. */}
      <mesh>
        <sphereGeometry args={[0.1, 10, 8]} />
        <meshBasicMaterial colorWrite={false} depthWrite={false} />
      </mesh>

      <mesh ref={ref}>
        <sphereGeometry args={[1, 16, 12]} />
        <meshBasicMaterial color={color} toneMapped={false} />
      </mesh>
      <mesh ref={ringRef} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.7, 1, 28]} />
        <meshBasicMaterial color={color} transparent opacity={0.3} toneMapped={false} />
      </mesh>

      {labelsVisible && (hovered || isActive) && (
        <Html center distanceFactor={2.6} position={[0, 0.12, 0]} zIndexRange={[20, 10]}>
          <div className="hotspot-label">{data.label}</div>
        </Html>
      )}
    </group>
  );
}

export function Hotspots({ step, onSelect, visible, onHoverChange }) {
  // DOM labels can't render inside an immersive session.
  const inXR = useXR((s) => !!s.session);
  if (!visible) return null;

  return (
    <group>
      {HOTSPOTS.map((h) => (
        <Hotspot
          key={h.label}
          data={h}
          isActive={h.step === step}
          onSelect={onSelect}
          labelsVisible={!inXR}
          onHoverChange={onHoverChange}
        />
      ))}
    </group>
  );
}
