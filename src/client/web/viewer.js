// SEADS web globe viewer — see index.html. DOWNSTREAM-ONLY presentation of a recorded flight.
//
// The interpolation here is a faithful JS mirror of the C++ layer-4a buffer (src/net/interp.cpp /
// tools/interp_ref.py): LINEAR lat & alt, SHORTEST-ARC lon (antimeridian) and bearing (360°),
// CLAMP/HOLD at the recording edges, sampled at a render time ~100 ms in the past. The globe
// frame matches src/client/globe.h: Y is the polar axis, lon 0 -> +X, lon +90° -> +Z.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const DEG = Math.PI / 180;
const DISPLAY_R = 10;                 // globe radius in scene units
const PALETTE = [0xffcf47, 0x7CFF6B, 0xff9a3c, 0xc792ff, 0x4f9dff, 0xff6b9d];

const traj = window.SEADS_TRAJECTORY;
const errEl = document.getElementById('err');
if (!traj || !traj.frames || !traj.frames.length) {
  errEl.style.display = 'grid';
  errEl.innerHTML = 'No trajectory loaded.<br/>Generate one with ' +
    '<code>seads_record --demo --js src/client/web/trajectory.js</code> next to this page.';
  throw new Error('no trajectory');
}

const frames = traj.frames;                       // [{tick, e:[{id,lat,lon,brg,alt,phi,tas}]}]
const radiusM = traj.meta.radius_m || 15000;
const tickHz = traj.meta.tick_hz || 100;
const snapHz = traj.meta.snap_hz || 20;
const typeNames = traj.meta.types || [];  // airframe display names per aircraft slot (v3 meta)
const scale = DISPLAY_R / radiusM;
const firstTick = frames[0].tick;
const lastTick = frames[frames.length - 1].tick;
const span = Math.max(1, lastTick - firstTick);
const delayTicks = 1.5 * (tickHz / snapHz);       // ~100 ms @ 20 Hz, matches Playback::delay_ticks

// ---- interpolation (bit-faithful to interp_ref.py op order) ---------------------------------
const lerp = (a, b, al) => a + (b - a) * al;
function lerpLon(a, b, al) {
  let d = b - a; if (d > 180) d -= 360; else if (d <= -180) d += 360;
  let r = a + d * al; if (r > 180) r -= 360; if (r <= -180) r += 360; return r;
}
function lerpAngle(a, b, al) {
  let d = b - a; if (d > 180) d -= 360; else if (d <= -180) d += 360;
  let r = a + d * al; if (r < 0) r += 360; if (r >= 360) r -= 360; return r;
}
function frameIndexFor(rt) {                       // newest frame with tick <= rt
  let i = 0;
  while (i + 1 < frames.length && frames[i + 1].tick <= rt) i++;
  return i;
}
function sample(rt) {
  if (rt <= firstTick) return frames[0].e.map(e => ({ ...e }));
  if (rt >= lastTick) return frames[frames.length - 1].e.map(e => ({ ...e }));
  const i = frameIndexFor(rt);
  const f0 = frames[i], f1 = frames[i + 1];
  const al = (rt - f0.tick) / (f1.tick - f0.tick);
  return f0.e.map(a => {
    const b = f1.e.find(x => x.id === a.id) || a;
    return {
      id: a.id,
      lat: lerp(a.lat, b.lat, al),
      lon: lerpLon(a.lon, b.lon, al),
      brg: lerpAngle(a.brg, b.brg, al),
      alt: lerp(a.alt, b.alt, al),
      phi: lerp(a.phi, b.phi, al),
      tas: lerp(a.tas, b.tas, al),
    };
  });
}

// ---- geometry helpers (match globe.h) -------------------------------------------------------
function geoToVec(latDeg, lonDeg, altM) {
  const la = latDeg * DEG, lo = lonDeg * DEG;
  const r = (radiusM + altM) * scale;
  const cl = Math.cos(la);
  return new THREE.Vector3(r * cl * Math.cos(lo), r * Math.sin(la), r * cl * Math.sin(lo));
}
// Local tangent heading direction (bearing from north toward east) for the nose tick.
function headingDir(latDeg, lonDeg, brgDeg) {
  const la = latDeg * DEG, lo = lonDeg * DEG, b = brgDeg * DEG;
  const north = new THREE.Vector3(-Math.sin(la) * Math.cos(lo), Math.cos(la),
                                  -Math.sin(la) * Math.sin(lo));
  const east = new THREE.Vector3(-Math.sin(lo), 0, Math.cos(lo));
  return north.multiplyScalar(Math.cos(b)).add(east.multiplyScalar(Math.sin(b))).normalize();
}

// ---- scene ----------------------------------------------------------------------------------
const app = document.getElementById('app');
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
app.appendChild(renderer.domElement);

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x05070d);
const camera = new THREE.PerspectiveCamera(45, innerWidth / innerHeight, 0.1, 1000);
camera.position.set(DISPLAY_R * 1.1, DISPLAY_R * 1.0, DISPLAY_R * 1.9);
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.minDistance = DISPLAY_R * 1.2;
controls.maxDistance = DISPLAY_R * 6;
// Auto-frame: aim the camera at the action (centroid of the opening frame, averaged as unit vectors
// so longitude wrap is handled) so the dogfight is centred on load for ANY recording. Drag to orbit,
// scroll to zoom.
{
  let vx = 0, vy = 0, vz = 0;
  for (const e of frames[0].e) { const v = geoToVec(e.lat, e.lon, 0); vx += v.x; vy += v.y; vz += v.z; }
  const dir = new THREE.Vector3(vx, vy, vz);
  if (dir.lengthSq() > 1e-9) {
    dir.normalize();
    camera.position.copy(dir.multiplyScalar(DISPLAY_R * 2.15)).add(new THREE.Vector3(0, DISPLAY_R * 0.12, 0));
    controls.update();
  }
}

scene.add(new THREE.AmbientLight(0x6688bb, 0.7));
const key = new THREE.DirectionalLight(0xffffff, 1.0); key.position.set(5, 8, 6); scene.add(key);

// Globe: a soft filled sphere + a wire shell + the ATM ceiling shell at 8000 m.
const globe = new THREE.Mesh(
  new THREE.SphereGeometry(DISPLAY_R, 48, 48),
  new THREE.MeshStandardMaterial({ color: 0x16314f, roughness: 0.95, metalness: 0.0 }));
scene.add(globe);
scene.add(new THREE.Mesh(new THREE.SphereGeometry(DISPLAY_R * 1.001, 24, 24),
  new THREE.MeshBasicMaterial({ color: 0x2f5f93, wireframe: true, transparent: true, opacity: 0.35 })));
const ceilR = (radiusM + 8000) * scale;
scene.add(new THREE.Mesh(new THREE.SphereGeometry(ceilR, 24, 24),
  new THREE.MeshBasicMaterial({ color: 0x35507a, wireframe: true, transparent: true, opacity: 0.08 })));
// Equator + prime meridian rings for orientation.
function ring(rot) {
  const g = new THREE.RingGeometry(DISPLAY_R * 1.002, DISPLAY_R * 1.004, 96);
  const m = new THREE.MeshBasicMaterial({ color: 0x3d6da8, side: THREE.DoubleSide,
    transparent: true, opacity: 0.5 });
  const r = new THREE.Mesh(g, m); r.rotation.x = rot.x; r.rotation.y = rot.y; scene.add(r);
}
ring({ x: Math.PI / 2, y: 0 });   // equator (XZ plane)
ring({ x: 0, y: Math.PI / 2 });   // prime meridian (XY plane)
// Starfield.
{
  const N = 1200, pos = new Float32Array(N * 3);
  let s = 1234567;                                   // deterministic LCG, no Math.random dependency
  const rnd = () => ((s = (s * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
  for (let i = 0; i < N; i++) {
    const u = rnd() * 2 - 1, th = rnd() * Math.PI * 2, rr = Math.sqrt(1 - u * u), R = 120 + rnd() * 60;
    pos.set([R * rr * Math.cos(th), R * u, R * rr * Math.sin(th)], i * 3);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  scene.add(new THREE.Points(g, new THREE.PointsMaterial({ color: 0x6f7fa8, size: 0.5 })));
}

// Per-aircraft visuals: marker, heading tick, trail.
const ids = frames[0].e.map(e => e.id);
const craft = ids.map((id, i) => {
  const color = PALETTE[i % PALETTE.length];
  const marker = new THREE.Mesh(new THREE.SphereGeometry(0.13, 16, 16),
    new THREE.MeshStandardMaterial({ color, emissive: color, emissiveIntensity: 0.5 }));
  scene.add(marker);
  const nose = new THREE.Line(new THREE.BufferGeometry().setFromPoints(
    [new THREE.Vector3(), new THREE.Vector3()]),
    new THREE.LineBasicMaterial({ color: 0xffffff }));
  scene.add(nose);
  const MAXT = 600;
  const trailPos = new Float32Array(MAXT * 3);
  const tg = new THREE.BufferGeometry();
  tg.setAttribute('position', new THREE.BufferAttribute(trailPos, 3));
  tg.setDrawRange(0, 0);
  const trail = new THREE.Line(tg, new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.7 }));
  scene.add(trail);
  return { id, color, marker, nose, trail, trailPos, trailLen: 0, MAXT };
});

// Tracer rounds (G1→G3): a point cloud refreshed each frame from the kernel-captured projectiles.
const MAXP = 600;
const tracerPos = new Float32Array(MAXP * 3);
const tracerGeo = new THREE.BufferGeometry();
tracerGeo.setAttribute('position', new THREE.BufferAttribute(tracerPos, 3));
tracerGeo.setDrawRange(0, 0);
const tracers = new THREE.Points(tracerGeo,
  new THREE.PointsMaterial({ color: 0xffe14a, size: 0.16, sizeAttenuation: true,
                             transparent: true, opacity: 0.95 }));
scene.add(tracers);
// Per-aircraft starting hp (max), for the HUD bar; hp is captured per frame.
const maxHp = (frames[0].hp || frames[0].e.map(() => 100)).slice();

// HUD rows.
const craftEl = document.getElementById('craft');
const rows = craft.map(c => {
  const row = document.createElement('div'); row.className = 'row';
  row.innerHTML = `<span class="dot" style="background:#${c.color.toString(16).padStart(6, '0')}"></span><span></span>`;
  craftEl.appendChild(row); return row.lastChild;
});
document.getElementById('meta').textContent =
  `${traj.meta.scenario} · ${frames.length} frames · ${snapHz}Hz snaps · ${(delayTicks / tickHz * 1000).toFixed(0)}ms interp delay · R=${radiusM}m`;

// ---- playback loop --------------------------------------------------------------------------
let playing = true, speed = 1, played = 0;          // played = ticks of playback progressed
let last = performance.now();
const playBtn = document.getElementById('play');
const scrub = document.getElementById('scrub');
const clockEl = document.getElementById('clock');
playBtn.onclick = () => { playing = !playing; playBtn.textContent = playing ? '⏸ pause' : '▶ play'; };
document.getElementById('speed').onchange = e => speed = parseFloat(e.target.value);
scrub.oninput = () => { played = (scrub.value / 1000) * span; playing = false; playBtn.textContent = '▶ play'; };
addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

function resetTrails() { for (const c of craft) { c.trailLen = 0; c.trail.geometry.setDrawRange(0, 0); } }

function frame(now) {
  // Clamp dt so a backgrounded/resumed tab (rAF paused) doesn't leap the playback forward.
  const dt = Math.min((now - last) / 1000, 0.1); last = now;
  if (playing) {
    played += dt * speed * tickHz;
    if (played >= span) { played = played % span; resetTrails(); }
    scrub.value = String((played / span) * 1000);
  }
  const renderTick = firstTick + Math.max(0, played - delayTicks);
  const ents = sample(renderTick);
  const fi = frameIndexFor(Math.max(firstTick, Math.min(lastTick, renderTick)));
  const hpNow = frames[fi].hp || maxHp;
  const ammoNow = frames[fi].ammo || [];    // v1.14r0: rounds remaining (wire-sourced)
  const killsNow = frames[fi].kills || [];  // v1.19r0: the wire-sourced scoreboard
  clockEl.textContent = (played / tickHz).toFixed(2) + 's';

  ents.forEach((e, i) => {
    const c = craft[i];
    const hp = hpNow[i] !== undefined ? hpNow[i] : maxHp[i];
    const dead = hp <= 0;
    const p = geoToVec(e.lat, e.lon, e.alt);
    c.marker.position.copy(p);
    // dead aircraft greys out and stops emitting; living ones keep their colour.
    c.marker.material.color.setHex(dead ? 0x444444 : c.color);
    c.marker.material.emissive.setHex(dead ? 0x000000 : c.color);
    c.marker.material.emissiveIntensity = dead ? 0.0 : 0.5;
    const dir = headingDir(e.lat, e.lon, e.brg);
    const tip = p.clone().add(dir.multiplyScalar(0.7));
    c.nose.geometry.setFromPoints([p, tip]);
    // append to trail (freeze the trail once dead)
    if (!dead) {
      if (c.trailLen < c.MAXT) {
        c.trailPos.set([p.x, p.y, p.z], c.trailLen * 3); c.trailLen++;
      } else {
        c.trailPos.copyWithin(0, 3); c.trailPos.set([p.x, p.y, p.z], (c.MAXT - 1) * 3);
      }
      c.trail.geometry.attributes.position.needsUpdate = true;
      c.trail.geometry.setDrawRange(0, c.trailLen);
    }
    const bar = (() => {                                   // tiny text hp bar
      const w = 10, n = Math.max(0, Math.round((hp / maxHp[i]) * w));
      return '█'.repeat(n) + '░'.repeat(w - n);
    })();
    // ammo counter + kill tally, when the recording carries them (wire fields v1.14r0/v1.19r0).
    const extra = (ammoNow[i] !== undefined ? `  ammo ${String(ammoNow[i]).padStart(3)}` : '') +
                  (killsNow[i] !== undefined ? `  kills ${killsNow[i]}` : '');
    const who = `#${e.id}` + (typeNames[i] ? ` ${typeNames[i]}` : '');
    rows[i].textContent = dead
      ? `${who}  ☠ KILLED   hp ${bar} 0/${maxHp[i].toFixed(0)}` +
        (killsNow[i] !== undefined ? `  kills ${killsNow[i]}` : '')
      : `${who}  alt ${e.alt.toFixed(0).padStart(5)}m  brg ${e.brg.toFixed(0).padStart(3)}  ` +
        `tas ${e.tas.toFixed(0).padStart(3)}  hp ${bar} ${hp.toFixed(0)}/${maxHp[i].toFixed(0)}` + extra;
  });

  // tracer rounds for this frame (snap to the captured frame; identity isn't tracked across frames)
  const pr = frames[fi].p || [];
  const np = Math.min(pr.length, MAXP);
  for (let i = 0; i < np; i++) {
    const v = geoToVec(pr[i][0], pr[i][1], pr[i][2]);
    tracerPos.set([v.x, v.y, v.z], i * 3);
  }
  tracerGeo.setDrawRange(0, np);
  tracerGeo.attributes.position.needsUpdate = true;

  controls.update();
  renderer.render(scene, camera);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

// Expose for headless screenshot/debug.
window.__SEADS_VIEWER__ = { sample, geoToVec, frames, delayTicks, get played() { return played; },
  set played(v) { played = v; } };
