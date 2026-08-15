import * as THREE from 'three';

const UP = new THREE.Vector3(0, 1, 0);
const _dir = new THREE.Vector3();
const _mid = new THREE.Vector3();
const _perp = new THREE.Vector3();
const _proj = new THREE.Vector3();

// Stretches a unit cylinder (radius 1, height 1, centered on its origin) so it
// spans from a to b. Every limb in the scene is drawn this way.
export function setBone(mesh, a, b, radius) {
  if (!mesh) return;
  _dir.subVectors(b, a);
  const len = _dir.length() || 0.0001;
  _mid.copy(a).addScaledVector(_dir, 0.5);
  mesh.position.copy(_mid);
  mesh.quaternion.setFromUnitVectors(UP, _dir.divideScalar(len));
  mesh.scale.set(radius, len, radius);
}

export function setJoint(mesh, p, radius) {
  if (!mesh) return;
  mesh.position.copy(p);
  mesh.scale.setScalar(radius);
}

// Two-bone IK: given a shoulder and a hand, find the elbow. `pole` biases which
// way the joint bends so elbows and knees don't snap to arbitrary directions.
export function solveElbow(out, shoulder, hand, upperLen, lowerLen, pole) {
  _dir.subVectors(hand, shoulder);
  const reach = Math.min(_dir.length(), (upperLen + lowerLen) * 0.999) || 0.0001;
  _dir.normalize();

  const along = (upperLen * upperLen - lowerLen * lowerLen + reach * reach) / (2 * reach);
  const lift = Math.sqrt(Math.max(0, upperLen * upperLen - along * along));

  _proj.copy(_dir).multiplyScalar(pole.dot(_dir));
  _perp.copy(pole).sub(_proj);
  if (_perp.lengthSq() < 1e-6) _perp.set(0, -1, 0);
  _perp.normalize();

  return out.copy(shoulder).addScaledVector(_dir, along).addScaledVector(_perp, lift);
}

export const damp = THREE.MathUtils.damp;

// Eases a Vector3 toward a target array without allocating.
export function dampVec(current, target, lambda, dt) {
  current.x = damp(current.x, target[0], lambda, dt);
  current.y = damp(current.y, target[1], lambda, dt);
  current.z = damp(current.z, target[2], lambda, dt);
}
