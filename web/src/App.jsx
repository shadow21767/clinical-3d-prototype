import { useCallback, useEffect, useMemo, useState } from 'react';
import { Canvas } from '@react-three/fiber';
import { XR, createXRStore } from '@react-three/xr';
import { Scene } from './scene/Scene';
import { STEPS } from './steps';

export default function App() {
  const [step, setStep] = useState(0);
  const [following, setFollowing] = useState(true);
  const [showHotspots, setShowHotspots] = useState(true);
  const [xrMode, setXrMode] = useState(null); // 'vr' | 'ar' | null
  const [inSession, setInSession] = useState(false);

  const store = useMemo(() => createXRStore(), []);
  const last = STEPS.length - 1;

  // Changing step always resumes the guided framing.
  const goTo = useCallback(
    (i) => {
      setStep(Math.max(0, Math.min(i, STEPS.length - 1)));
      setFollowing(true);
    },
    [],
  );
  const next = useCallback(() => goTo(step + 1), [goTo, step]);
  const prev = useCallback(() => goTo(step - 1), [goTo, step]);

  useEffect(() => {
    const onKey = (e) => {
      if (e.key === 'ArrowRight' || e.key === ' ') {
        e.preventDefault();
        next();
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        prev();
      } else if (e.key === 'r' || e.key === 'R') {
        setFollowing(true);
      } else if (e.key === 'h' || e.key === 'H') {
        setShowHotspots((v) => !v);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [next, prev]);

  // Probe once for headset support so the button reflects reality.
  useEffect(() => {
    let alive = true;
    const xr = navigator.xr;
    if (!xr?.isSessionSupported) return undefined;
    (async () => {
      try {
        if (await xr.isSessionSupported('immersive-vr')) {
          if (alive) setXrMode('vr');
          return;
        }
        if (await xr.isSessionSupported('immersive-ar')) {
          if (alive) setXrMode('ar');
        }
      } catch {
        /* unsupported browser; button stays hidden */
      }
    })();
    return () => {
      alive = false;
    };
  }, []);

  useEffect(() => store.subscribe((s) => setInSession(!!s.session)), [store]);

  const enterXR = () => {
    if (xrMode === 'vr') store.enterVR();
    else if (xrMode === 'ar') store.enterAR();
  };

  const s = STEPS[step];

  return (
    <div className="app">
      <Canvas shadows camera={{ fov: 42, position: [-2.95, 1.8, 2.45] }} dpr={[1, 2]}>
        <color attach="background" args={['#0b1016']} />
        <XR store={store}>
          <Scene
            step={step}
            steps={STEPS}
            following={following}
            onTakeOver={() => setFollowing(false)}
            onSelect={goTo}
            showHotspots={showHotspots}
          />
        </XR>
      </Canvas>

      <header className="topbar">
        <div className="brand">
          <span className="dot" />
          Clinical Procedure Walkthrough
        </div>
        <div className="case">Trauma primary survey &middot; adult, single responder</div>
      </header>

      <nav className="rail" aria-label="Procedure steps">
        {STEPS.map((item, i) => (
          <button
            key={item.id}
            className={`rail-item ${i === step ? 'is-active' : ''} ${i < step ? 'is-done' : ''}`}
            onClick={() => goTo(i)}
          >
            <span className="rail-num">{i + 1}</span>
            <span className="rail-label">{item.title}</span>
          </button>
        ))}
      </nav>

      <div className="viewbar">
        <span className={`mode ${following ? '' : 'is-free'}`}>
          {following ? 'Guided view' : 'Free look'}
        </span>
        <button className="chip" onClick={() => setFollowing(true)} disabled={following}>
          Reset view
        </button>
        <button className="chip" onClick={() => setShowHotspots((v) => !v)}>
          {showHotspots ? 'Hide hotspots' : 'Show hotspots'}
        </button>
        {xrMode && (
          <button className="chip accent" onClick={enterXR}>
            {inSession ? 'In session' : xrMode === 'vr' ? 'Enter VR' : 'Enter AR'}
          </button>
        )}
        <span className="viewhint">Drag to orbit &middot; scroll to zoom &middot; click a marker</span>
      </div>

      <section className="panel">
        <div className="panel-head">
          <span className="phase">{s.phase}</span>
          <span className="count">
            Step {step + 1} of {STEPS.length}
          </span>
        </div>

        <h1>{s.title}</h1>
        <p className="narration">{s.narration}</p>

        <h2>Key actions</h2>
        <ul className="actions">
          {s.actions.map((a) => (
            <li key={a}>{a}</li>
          ))}
        </ul>

        <div className="watch">
          <strong>Watch for</strong>
          {s.watch}
        </div>

        <div className="controls">
          <button className="btn ghost" onClick={prev} disabled={step === 0}>
            Back
          </button>
          <button className="btn primary" onClick={next} disabled={step === last}>
            {step === last ? 'Complete' : 'Next step'}
          </button>
        </div>

        <div className="progress">
          <div className="progress-fill" style={{ width: `${((step + 1) / STEPS.length) * 100}%` }} />
        </div>
        <div className="hint">Arrows or space to advance &middot; R resets the view &middot; H toggles hotspots</div>
      </section>
    </div>
  );
}
