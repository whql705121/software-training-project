import * as THREE from "three";
import { OrbitControls } from "./vendor/OrbitControls.js";

const canvas = document.querySelector("#scene");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: "high-performance" });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.outputColorSpace = THREE.SRGBColorSpace;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0xdbe7f4);
scene.fog = new THREE.Fog(0xdbe7f4, 68, 132);

const camera = new THREE.PerspectiveCamera(48, window.innerWidth / window.innerHeight, 0.1, 220);
camera.position.set(30, 32, 42);

const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(0, 0, 1.8);
controls.enableDamping = true;
controls.dampingFactor = 0.06;
controls.maxPolarAngle = Math.PI * 0.47;
controls.minDistance = 18;
controls.maxDistance = 88;
controls.panSpeed = 0.55;

const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
const clock = new THREE.Clock();

const ui = {
  clock: document.querySelector("#clock"),
  statusText: document.querySelector("#statusText"),
  activeCount: document.querySelector("#activeCount"),
  queueCount: document.querySelector("#queueCount"),
  seatRate: document.querySelector("#seatRate"),
  servedCount: document.querySelector("#servedCount"),
  throughput: document.querySelector("#throughput"),
  trendChart: document.querySelector("#trendChart"),
  windowChart: document.querySelector("#windowChart"),
  stateChart: document.querySelector("#stateChart"),
  pauseBtn: document.querySelector("#pauseBtn"),
  resetBtn: document.querySelector("#resetBtn"),
  cameraBtn: document.querySelector("#cameraBtn"),
  speedRange: document.querySelector("#speedRange"),
  heatToggle: document.querySelector("#heatToggle"),
};

const colors = {
  floor: 0xf4f7fb,
  wall: 0xd7e2f0,
  blue: 0x3b82f6,
  amber: 0xf59e0b,
  green: 0x10b981,
  violet: 0x8b5cf6,
  red: 0xef4444,
  ink: 0x172033,
};

const layout = {
  entrance: new THREE.Vector3(-27, 0, 12),
  pickup: new THREE.Vector3(-10, 0, -1),
  exit: new THREE.Vector3(28, 0, 12),
  rest: new THREE.Vector3(15, 0, 12),
  waitingBase: new THREE.Vector3(-3, 0, 10),
  windows: [
    { name: "快餐 1", pos: new THREE.Vector3(-24, 0, -12), color: colors.blue, queue: [], current: null },
    { name: "风味 2", pos: new THREE.Vector3(-24, 0, -5), color: colors.green, queue: [], current: null },
    { name: "快餐 3", pos: new THREE.Vector3(-24, 0, 2), color: colors.blue, queue: [], current: null },
    { name: "风味 4", pos: new THREE.Vector3(-24, 0, 9), color: colors.green, queue: [], current: null },
  ],
  seats: [],
};

const sim = {
  time: 0,
  nextArrival: 0.4,
  nextId: 1,
  students: [],
  served: 0,
  left: 0,
  rested: 0,
  paused: false,
  speed: 1.3,
  history: [],
};

const pools = {
  studentMeshes: [],
};

const groups = {
  static: new THREE.Group(),
  dynamic: new THREE.Group(),
  heat: new THREE.Group(),
  labels: new THREE.Group(),
  trails: new THREE.Group(),
};
scene.add(groups.static, groups.dynamic, groups.heat, groups.labels, groups.trails);

function makeMaterial(color, roughness = 0.68, metalness = 0.02) {
  return new THREE.MeshStandardMaterial({ color, roughness, metalness });
}

function roundedBox(width, height, depth, color) {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(width, height, depth), makeMaterial(color));
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  return mesh;
}

function addBox(width, height, depth, color, position, parent = groups.static) {
  const mesh = roundedBox(width, height, depth, color);
  mesh.position.copy(position);
  parent.add(mesh);
  return mesh;
}

function addLabel(text, position, scale = 1, color = "#172033") {
  const size = 256;
  const canvas2d = document.createElement("canvas");
  canvas2d.width = size * 2;
  canvas2d.height = size;
  const ctx = canvas2d.getContext("2d");
  ctx.clearRect(0, 0, canvas2d.width, canvas2d.height);
  ctx.font = "700 42px Microsoft YaHei UI, sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillStyle = "rgba(255,255,255,0.86)";
  roundRect2d(ctx, 22, 64, canvas2d.width - 44, 86, 28);
  ctx.fill();
  ctx.strokeStyle = "rgba(45,57,79,0.16)";
  ctx.lineWidth = 3;
  roundRect2d(ctx, 22, 64, canvas2d.width - 44, 86, 28);
  ctx.stroke();
  ctx.fillStyle = color;
  ctx.fillText(text, canvas2d.width / 2, canvas2d.height / 2 + 2);

  const texture = new THREE.CanvasTexture(canvas2d);
  texture.colorSpace = THREE.SRGBColorSpace;
  const material = new THREE.SpriteMaterial({ map: texture, transparent: true });
  const sprite = new THREE.Sprite(material);
  sprite.position.copy(position);
  sprite.scale.set(5.8 * scale, 2.9 * scale, 1);
  groups.labels.add(sprite);
  return sprite;
}

function roundRect2d(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

function makePathLine(points, color, opacity = 0.36) {
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const material = new THREE.LineDashedMaterial({
    color,
    dashSize: 0.55,
    gapSize: 0.38,
    transparent: true,
    opacity,
  });
  const line = new THREE.Line(geometry, material);
  line.computeLineDistances();
  groups.static.add(line);
  return line;
}

function buildLighting() {
  const hemi = new THREE.HemisphereLight(0xf7fbff, 0xaebed0, 2.3);
  scene.add(hemi);

  const sun = new THREE.DirectionalLight(0xffffff, 2.2);
  sun.position.set(-18, 35, 22);
  sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  sun.shadow.camera.left = -48;
  sun.shadow.camera.right = 48;
  sun.shadow.camera.top = 42;
  sun.shadow.camera.bottom = -42;
  scene.add(sun);

  const accent = new THREE.PointLight(0x6ea8ff, 22, 60, 1.8);
  accent.position.set(8, 10, -14);
  scene.add(accent);
}

function buildCafeteria() {
  addBox(62, 0.35, 36, colors.floor, new THREE.Vector3(0, -0.18, 0));
  addBox(64, 2.2, 0.55, colors.wall, new THREE.Vector3(0, 1.1, -18.2));
  addBox(64, 2.2, 0.55, colors.wall, new THREE.Vector3(0, 1.1, 18.2));
  addBox(0.55, 2.2, 36, colors.wall, new THREE.Vector3(-32.2, 1.1, 0));
  addBox(0.55, 2.2, 36, colors.wall, new THREE.Vector3(32.2, 1.1, 0));

  addBox(6, 0.18, 5.4, 0xdff7ec, new THREE.Vector3(layout.entrance.x, 0.02, layout.entrance.z));
  addBox(6, 0.18, 5.4, 0xffeadf, new THREE.Vector3(layout.exit.x, 0.02, layout.exit.z));
  addBox(13, 0.12, 9.8, 0xeaf4ff, new THREE.Vector3(layout.rest.x, 0.03, layout.rest.z));
  addBox(7.8, 0.12, 6.4, 0xfff5d7, new THREE.Vector3(layout.waitingBase.x, 0.04, layout.waitingBase.z));

  addLabel("入口", new THREE.Vector3(layout.entrance.x, 3.2, layout.entrance.z), 0.65, "#166534");
  addLabel("出口", new THREE.Vector3(layout.exit.x, 3.2, layout.exit.z), 0.65, "#9a3412");
  addLabel("等座区", new THREE.Vector3(layout.waitingBase.x, 2.6, layout.waitingBase.z + 2.6), 0.55, "#854d0e");
  addLabel("休息区", new THREE.Vector3(layout.rest.x, 2.6, layout.rest.z + 3.6), 0.55, "#1d4ed8");

  layout.windows.forEach((win, index) => {
    addBox(5.8, 2.9, 3.4, win.color, new THREE.Vector3(win.pos.x, 1.45, win.pos.z));
    addBox(0.38, 0.25, 4.2, 0xffffff, new THREE.Vector3(win.pos.x + 3.2, 1.8, win.pos.z));
    addLabel(win.name, new THREE.Vector3(win.pos.x, 4.1, win.pos.z), 0.5, "#ffffff");
    makePathLine([
      layout.entrance.clone().setY(0.08),
      queueSlot(index, 3).setY(0.08),
      servicePoint(index).setY(0.08),
      layout.pickup.clone().setY(0.08),
    ], win.color, 0.32);
  });

  makePathLine([
    layout.pickup.clone().setY(0.08),
    new THREE.Vector3(4, 0.08, -3),
    new THREE.Vector3(14, 0.08, 0),
    layout.exit.clone().setY(0.08),
  ], colors.amber, 0.42);

  makeSeats("A", -2, -12, 3, 5, 0xcee4ff);
  makeSeats("B", -2, 0, 3, 5, 0xd6f5e7);
  makeSeats("C", 14, -10, 4, 4, 0xe4d8ff);

  addLabel("A 区 近窗口", new THREE.Vector3(3.4, 3, -14.5), 0.5, "#1d4ed8");
  addLabel("B 区 安静区", new THREE.Vector3(3.4, 3, -2.5), 0.5, "#047857");
  addLabel("C 区 大桌区", new THREE.Vector3(19.5, 3, -12.5), 0.5, "#6d28d9");
}

function makeSeats(zone, startX, startZ, rows, cols, matColor) {
  for (let r = 0; r < rows; r += 1) {
    for (let c = 0; c < cols; c += 1) {
      const x = startX + c * 2.7;
      const z = startZ + r * 2.8;
      const table = addBox(1.65, 0.32, 1.1, 0xf8fafc, new THREE.Vector3(x, 0.58, z));
      table.material = makeMaterial(0xfafafa, 0.44);
      const seat = addBox(1.05, 0.36, 0.9, matColor, new THREE.Vector3(x, 0.33, z + 1.05));
      seat.userData.baseColor = matColor;
      seat.userData.zone = zone;
      layout.seats.push({ zone, pos: new THREE.Vector3(x, 0, z + 1.05), mesh: seat, occupant: null });
    }
  }
}

function queueSlot(windowIndex, slotIndex) {
  const win = layout.windows[windowIndex];
  return new THREE.Vector3(win.pos.x + 5 + slotIndex * 1.7, 0, win.pos.z);
}

function servicePoint(windowIndex) {
  const win = layout.windows[windowIndex];
  return new THREE.Vector3(win.pos.x + 3.8, 0, win.pos.z);
}

function makeStudentMesh(student) {
  const group = new THREE.Group();
  const material = makeMaterial(student.color, 0.56);
  const body = new THREE.Mesh(new THREE.CylinderGeometry(0.34, 0.42, 1.1, 18), material);
  body.position.y = 0.75;
  body.castShadow = true;
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.36, 20, 20), makeMaterial(0xffd9b3, 0.62));
  head.position.y = 1.48;
  head.castShadow = true;
  const bag = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.46, 0.5), makeMaterial(0x263245, 0.7));
  bag.position.set(0, 0.88, -0.42);
  bag.castShadow = true;
  const ring = new THREE.Mesh(new THREE.TorusGeometry(0.55, 0.035, 8, 32), makeMaterial(student.color, 0.4));
  ring.rotation.x = Math.PI / 2;
  ring.position.y = 0.05;
  group.add(body, head, bag, ring);
  group.position.copy(student.pos);
  groups.dynamic.add(group);
  return group;
}

function resetSimulation() {
  sim.time = 0;
  sim.nextArrival = 0.2;
  sim.nextId = 1;
  sim.served = 0;
  sim.left = 0;
  sim.rested = 0;
  sim.history.length = 0;
  layout.windows.forEach((win) => {
    win.queue.length = 0;
    win.current = null;
  });
  layout.seats.forEach((seat) => {
    seat.occupant = null;
    seat.mesh.material.color.setHex(seat.mesh.userData.baseColor);
    seat.mesh.position.y = 0.33;
  });
  sim.students.forEach((student) => groups.dynamic.remove(student.mesh));
  sim.students.length = 0;
  seedInitialCrowd();
  recordMetrics();
}

function chooseWindow() {
  let best = 0;
  let bestLoad = Infinity;
  layout.windows.forEach((win, index) => {
    const load = win.queue.length + (win.current ? 1 : 0);
    if (load < bestLoad) {
      best = index;
      bestLoad = load;
    }
  });
  return best;
}

function createStudent({ pos, target, state, windowIndex = -1, seatIndex = -1, eatUntil = 0, restUntil = 0 }) {
  const palette = [colors.blue, colors.green, colors.violet, colors.red];
  const student = {
    id: sim.nextId++,
    type: Math.floor(Math.random() * 3),
    color: palette[Math.floor(Math.random() * palette.length)],
    pos: pos.clone(),
    target: target.clone(),
    speed: 4.5 + Math.random() * 1.8,
    state,
    windowIndex,
    seatIndex,
    serviceUntil: 0,
    eatUntil,
    restUntil,
    trail: [],
  };
  student.mesh = makeStudentMesh(student);
  sim.students.push(student);
  return student;
}

function spawnStudent() {
  const windowIndex = chooseWindow();
  const student = createStudent({
    pos: layout.entrance,
    target: queueSlot(windowIndex, layout.windows[windowIndex].queue.length),
    state: "toWindow",
    windowIndex,
  });
  layout.windows[windowIndex].queue.push(student);
}

function seedInitialCrowd() {
  layout.windows.forEach((win, windowIndex) => {
    const service = createStudent({
      pos: servicePoint(windowIndex),
      target: servicePoint(windowIndex),
      state: "service",
      windowIndex,
    });
    service.serviceUntil = sim.time + 1.2 + Math.random() * 1.8;
    win.current = service;

    for (let i = 0; i < 2; i += 1) {
      const student = createStudent({
        pos: queueSlot(windowIndex, i),
        target: queueSlot(windowIndex, i),
        state: "queueing",
        windowIndex,
      });
      win.queue.push(student);
    }
  });

  for (let i = 0; i < 9; i += 1) {
    const seat = layout.seats.find((candidate) => !candidate.occupant);
    if (!seat) break;
    const seatIndex = layout.seats.indexOf(seat);
    const student = createStudent({
      pos: seat.pos,
      target: seat.pos,
      state: "eating",
      seatIndex,
      eatUntil: sim.time + 4 + Math.random() * 8,
    });
    seat.occupant = student;
    seat.mesh.material.color.setHex(0x54a3ff);
    seat.mesh.position.y = 0.48;
  }

  for (let i = 0; i < 3; i += 1) {
    const pos = layout.rest.clone().add(new THREE.Vector3(i * 1.6 - 2, 0, Math.random() * 2 - 1));
    createStudent({
      pos,
      target: pos,
      state: "resting",
      restUntil: sim.time + 2 + Math.random() * 4,
    });
  }
}

function findSeat(student) {
  const prefer = student.type === 0 ? "A" : student.type === 1 ? "B" : "C";
  let bestIndex = -1;
  let bestScore = Infinity;
  layout.seats.forEach((seat, index) => {
    if (seat.occupant) return;
    const zonePenalty = seat.zone === prefer ? 0 : 5;
    const score = seat.pos.distanceTo(layout.pickup) + zonePenalty + Math.random() * 1.4;
    if (score < bestScore) {
      bestIndex = index;
      bestScore = score;
    }
  });
  return bestIndex;
}

function moveToward(student, dt) {
  const distance = student.pos.distanceTo(student.target);
  if (distance < 0.03) {
    student.pos.copy(student.target);
    return true;
  }
  const step = Math.min(distance, student.speed * sim.speed * dt);
  const direction = student.target.clone().sub(student.pos).normalize();
  student.pos.addScaledVector(direction, step);
  student.mesh.position.copy(student.pos);
  student.mesh.lookAt(student.target.x, student.mesh.position.y, student.target.z);
  return distance < 0.18;
}

function updateQueues() {
  layout.windows.forEach((win, windowIndex) => {
    if (!win.current && win.queue.length > 0) {
      const candidate = win.queue[0];
      if (candidate.state === "queueing" && candidate.pos.distanceTo(queueSlot(windowIndex, 0)) < 0.35) {
        win.queue.shift();
        win.current = candidate;
        candidate.state = "service";
        candidate.target = servicePoint(windowIndex);
        candidate.serviceUntil = sim.time + 1.4 + Math.random() * 2.0;
      }
    }
    win.queue.forEach((student, index) => {
      student.target = queueSlot(windowIndex, index);
    });
  });
}

function assignSeat(student) {
  const seatIndex = findSeat(student);
  if (seatIndex === -1) {
    student.state = "waiting";
    student.target = layout.waitingBase.clone().add(new THREE.Vector3((student.id % 5) * 0.9 - 2, 0, (student.id % 3) * 0.9 - 1));
    return;
  }
  const seat = layout.seats[seatIndex];
  seat.occupant = student;
  student.seatIndex = seatIndex;
  student.state = "toSeat";
  student.target = seat.pos.clone();
  seat.mesh.material.color.setHex(0x54a3ff);
  seat.mesh.position.y = 0.48;
}

function updateSimulation(dt) {
  if (sim.paused) return;
  sim.time += dt * sim.speed;
  if (sim.time >= sim.nextArrival) {
    spawnStudent();
    sim.nextArrival = sim.time + 0.55 + Math.random() * 0.85;
  }

  updateQueues();

  sim.students.forEach((student) => {
    if (student.state === "done") return;
    const arrived = moveToward(student, dt);
    if (student.state === "toWindow" && arrived) {
      student.state = "queueing";
    } else if (student.state === "service" && sim.time >= student.serviceUntil && arrived) {
      layout.windows[student.windowIndex].current = null;
      sim.served += 1;
      student.target = layout.pickup.clone();
      assignSeat(student);
    } else if (student.state === "waiting" && Math.random() < 0.04) {
      assignSeat(student);
    } else if (student.state === "toSeat" && arrived) {
      student.state = "eating";
      student.eatUntil = sim.time + 4.0 + Math.random() * 7.5;
    } else if (student.state === "eating") {
      student.pos.copy(student.target);
      student.mesh.position.copy(student.pos);
      if (sim.time >= student.eatUntil) {
        const seat = layout.seats[student.seatIndex];
        if (seat) {
          seat.occupant = null;
          seat.mesh.material.color.setHex(seat.mesh.userData.baseColor);
          seat.mesh.position.y = 0.33;
        }
        if (Math.random() < 0.35) {
          student.state = "resting";
          student.target = layout.rest.clone().add(new THREE.Vector3(Math.random() * 8 - 4, 0, Math.random() * 3 - 1.5));
          student.restUntil = sim.time + 2 + Math.random() * 4;
          sim.rested += 1;
        } else {
          student.state = "leaving";
          student.target = layout.exit.clone();
        }
      }
    } else if (student.state === "resting" && sim.time >= student.restUntil && arrived) {
      student.state = "leaving";
      student.target = layout.exit.clone();
    } else if (student.state === "leaving" && arrived) {
      student.state = "done";
      sim.left += 1;
      groups.dynamic.remove(student.mesh);
    }
  });

  recordMetrics();
  updateHeat();
}

function currentMetrics() {
  const active = sim.students.filter((s) => s.state !== "done").length;
  const queue = layout.windows.reduce((sum, win) => sum + win.queue.length, 0);
  const service = layout.windows.reduce((sum, win) => sum + (win.current ? 1 : 0), 0);
  const occupied = layout.seats.filter((seat) => seat.occupant).length;
  const states = {
    service,
    eating: sim.students.filter((s) => s.state === "eating").length,
    waiting: sim.students.filter((s) => s.state === "waiting").length,
    resting: sim.students.filter((s) => s.state === "resting").length,
    leaving: sim.students.filter((s) => s.state === "leaving").length,
  };
  return {
    time: sim.time,
    arrivals: Math.max(0, sim.nextId - 1),
    active,
    queue,
    service,
    occupied,
    seatRate: occupied / layout.seats.length,
    served: sim.served,
    left: sim.left,
    throughput: sim.time > 0 ? sim.served * 60 / sim.time : 0,
    windows: layout.windows.map((win) => win.queue.length + (win.current ? 1 : 0)),
    states,
  };
}

function recordMetrics() {
  if (Math.floor(sim.time * 10) % 2 !== 0 && sim.history.length > 0) return;
  sim.history.push(currentMetrics());
  if (sim.history.length > 180) sim.history.shift();
}

function updateHeat() {
  groups.heat.visible = ui.heatToggle.checked;
  const m = currentMetrics();
  groups.heat.children.forEach((mesh, index) => {
    const value = index === 0 ? m.queue / 12 : index === 1 ? m.seatRate : m.states.waiting / 8;
    mesh.material.opacity = THREE.MathUtils.clamp(0.08 + value * 0.34, 0.08, 0.48);
    mesh.scale.y = 1 + THREE.MathUtils.clamp(value, 0, 1) * 0.04;
  });
}

function buildHeat() {
  [
    [-19, -2, 17, 18, colors.amber],
    [9, -6, 25, 22, colors.green],
    [-3, 10, 9, 7, colors.red],
  ].forEach(([x, z, w, d, color]) => {
    const mesh = new THREE.Mesh(
      new THREE.PlaneGeometry(w, d),
      new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.14, side: THREE.DoubleSide })
    );
    mesh.rotation.x = -Math.PI / 2;
    mesh.position.set(x, 0.085, z);
    groups.heat.add(mesh);
  });
}

function drawCharts() {
  const m = currentMetrics();
  ui.clock.textContent = formatTime(m.time);
  ui.statusText.textContent = sim.paused ? "已暂停" : "运行中";
  ui.activeCount.textContent = m.active;
  ui.queueCount.textContent = m.queue;
  ui.seatRate.textContent = `${Math.round(m.seatRate * 100)}%`;
  ui.servedCount.textContent = m.served;
  ui.throughput.textContent = `${Math.round(m.throughput)}/min`;
  drawTrend(ui.trendChart, sim.history);
  drawWindowChart(ui.windowChart, m.windows);
  drawStateChart(ui.stateChart, m.states);
}

function formatTime(seconds) {
  const min = Math.floor(seconds / 60).toString().padStart(2, "0");
  const sec = Math.floor(seconds % 60).toString().padStart(2, "0");
  return `${min}:${sec}`;
}

function prepCanvas(canvas) {
  const ctx = canvas.getContext("2d");
  const rect = canvas.getBoundingClientRect();
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const width = Math.max(1, Math.round(rect.width));
  const height = Math.max(1, Math.round(rect.height));
  const targetWidth = Math.round(width * dpr);
  const targetHeight = Math.round(height * dpr);
  if (canvas.width !== targetWidth || canvas.height !== targetHeight) {
    canvas.width = targetWidth;
    canvas.height = targetHeight;
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);
  return { ctx, width, height };
}

function drawTrend(canvas, history) {
  const { ctx, width, height } = prepCanvas(canvas);
  const latest = history[history.length - 1];
  if (!latest) return;

  const buckets = bucketHistory(history, 16);
  drawThroughputPanel(ctx, buckets, { x: 18, y: 20, w: width - 36, h: 106 });
  drawWindowHeatmap(ctx, buckets, { x: 18, y: 148, w: width - 36, h: 72 });
  drawOccupancyGauge(ctx, history, latest, { x: 18, y: height - 44, w: width - 36, h: 34 });
}

function bucketHistory(history, bucketCount) {
  const buckets = [];
  const size = Math.max(1, Math.ceil(history.length / bucketCount));
  for (let i = 0; i < history.length; i += size) {
    const slice = history.slice(i, i + size);
    const first = slice[0];
    const last = slice[slice.length - 1];
    const avgWindows = [0, 1, 2, 3].map((index) =>
      slice.reduce((sum, sample) => sum + (sample.windows[index] || 0), 0) / slice.length
    );
    buckets.push({
      arrivals: Math.max(0, last.arrivals - first.arrivals),
      served: Math.max(0, last.served - first.served),
      queue: slice.reduce((sum, sample) => sum + sample.queue + sample.service, 0) / slice.length,
      seatRate: slice.reduce((sum, sample) => sum + sample.seatRate, 0) / slice.length,
      windows: avgWindows,
    });
  }
  return buckets;
}

function drawPanelLabel(ctx, title, subtitle, x, y) {
  ctx.fillStyle = "#172033";
  ctx.font = "800 15px Microsoft YaHei UI";
  ctx.textAlign = "left";
  ctx.fillText(title, x, y);
  ctx.fillStyle = "#738096";
  ctx.font = "12px Microsoft YaHei UI";
  ctx.fillText(subtitle, x, y + 18);
}

function drawThroughputPanel(ctx, buckets, rect) {
  drawPanelLabel(ctx, "客流节奏", "进入 / 完成 / 排队压力", rect.x, rect.y);
  const plot = { x: rect.x, y: rect.y + 34, w: rect.w, h: rect.h - 36 };
  const maxBar = Math.max(1, ...buckets.map((b) => Math.max(b.arrivals, b.served)));
  const maxQueue = Math.max(1, ...buckets.map((b) => b.queue));
  const gap = 5;
  const groupW = plot.w / Math.max(1, buckets.length);
  const barW = Math.max(5, (groupW - gap) / 2);

  drawSoftGrid(ctx, plot, 3);
  buckets.forEach((bucket, index) => {
    const x = plot.x + index * groupW;
    const inH = Math.max(bucket.arrivals > 0 ? 8 : 0, plot.h * bucket.arrivals / maxBar);
    const servedH = Math.max(bucket.served > 0 ? 8 : 0, plot.h * bucket.served / maxBar);
    ctx.fillStyle = "rgba(59,130,246,.82)";
    roundRect2d(ctx, x + 1, plot.y + plot.h - inH, barW, inH, 6);
    ctx.fill();
    ctx.fillStyle = "rgba(139,92,246,.82)";
    roundRect2d(ctx, x + 1 + barW, plot.y + plot.h - servedH, barW, servedH, 6);
    ctx.fill();
  });

  ctx.strokeStyle = "#f59e0b";
  ctx.lineWidth = 4;
  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  ctx.shadowColor = "rgba(245,158,11,.35)";
  ctx.shadowBlur = 8;
  ctx.beginPath();
  buckets.forEach((bucket, index) => {
    const x = plot.x + index * groupW + groupW / 2;
    const y = plot.y + plot.h - plot.h * bucket.queue / maxQueue;
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
  ctx.shadowBlur = 0;

  const latest = buckets[buckets.length - 1] || { arrivals: 0, served: 0, queue: 0 };
  drawMiniBadge(ctx, rect.x + rect.w - 120, rect.y - 2, "#3b82f6", `进 ${latest.arrivals}`);
  drawMiniBadge(ctx, rect.x + rect.w - 62, rect.y - 2, "#8b5cf6", `完 ${latest.served}`);
}

function drawWindowHeatmap(ctx, buckets, rect) {
  drawPanelLabel(ctx, "窗口压力", "深色代表拥堵，按时间从左到右", rect.x, rect.y);
  const plot = { x: rect.x + 34, y: rect.y + 30, w: rect.w - 34, h: rect.h - 32 };
  const maxLoad = Math.max(1, ...buckets.flatMap((bucket) => bucket.windows));
  const cellGap = 4;
  const cellW = (plot.w - cellGap * (buckets.length - 1)) / Math.max(1, buckets.length);
  const cellH = (plot.h - cellGap * 3) / 4;

  for (let row = 0; row < 4; row += 1) {
    ctx.fillStyle = "#738096";
    ctx.font = "700 11px Microsoft YaHei UI";
    ctx.textAlign = "right";
    ctx.fillText(`W${row + 1}`, rect.x + 25, plot.y + row * (cellH + cellGap) + cellH * 0.72);
    buckets.forEach((bucket, index) => {
      const load = bucket.windows[row] || 0;
      const ratio = load / maxLoad;
      ctx.fillStyle = ratio > 0.66
        ? `rgba(239,68,68,${(0.22 + ratio * 0.68).toFixed(3)})`
        : `rgba(245,158,11,${(0.12 + ratio * 0.78).toFixed(3)})`;
      roundRect2d(
        ctx,
        plot.x + index * (cellW + cellGap),
        plot.y + row * (cellH + cellGap),
        Math.max(2, cellW),
        cellH,
        4
      );
      ctx.fill();
    });
  }
}

function drawOccupancyGauge(ctx, history, latest, rect) {
  const values = history.map((sample) => sample.seatRate * 100);
  const min = Math.round(Math.min(...values));
  const max = Math.round(Math.max(...values));
  const current = Math.round(latest.seatRate * 100);
  drawPanelLabel(ctx, "座位占用", `当前 ${current}%  波动 ${min}% - ${max}%`, rect.x, rect.y);

  const bar = { x: rect.x + 104, y: rect.y + 5, w: rect.w - 104, h: 22 };
  const gradient = ctx.createLinearGradient(bar.x, 0, bar.x + bar.w, 0);
  gradient.addColorStop(0, "#d9f99d");
  gradient.addColorStop(0.48, "#10b981");
  gradient.addColorStop(1, "#047857");
  ctx.fillStyle = "rgba(115,128,150,.12)";
  roundRect2d(ctx, bar.x, bar.y, bar.w, bar.h, 9);
  ctx.fill();
  ctx.fillStyle = gradient;
  roundRect2d(ctx, bar.x, bar.y, bar.w * latest.seatRate, bar.h, 9);
  ctx.fill();
  const markerX = bar.x + bar.w * latest.seatRate;
  ctx.fillStyle = "#172033";
  ctx.beginPath();
  ctx.arc(markerX, bar.y + bar.h / 2, 5, 0, Math.PI * 2);
  ctx.fill();
}

function drawMiniBadge(ctx, x, y, color, text) {
  ctx.save();
  ctx.font = "800 12px Microsoft YaHei UI";
  const w = ctx.measureText(text).width + 20;
  ctx.fillStyle = "rgba(255,255,255,.95)";
  roundRect2d(ctx, x, y, w, 24, 12);
  ctx.fill();
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(x + 10, y + 12, 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#172033";
  ctx.textAlign = "left";
  ctx.fillText(text, x + 17, y + 16);
  ctx.restore();
}

function drawSoftGrid(ctx, rect, rows) {
  ctx.strokeStyle = "rgba(115,128,150,.14)";
  ctx.lineWidth = 1;
  for (let i = 0; i <= rows; i += 1) {
    const y = rect.y + rect.h * i / rows;
    ctx.beginPath();
    ctx.moveTo(rect.x, y);
    ctx.lineTo(rect.x + rect.w, y);
    ctx.stroke();
  }
}

function drawWindowChart(canvas, loads) {
  const { ctx, width, height } = prepCanvas(canvas);
  const max = Math.max(1, ...loads);
  const colorsBar = ["#3b82f6", "#10b981", "#60a5fa", "#34d399"];
  loads.forEach((load, index) => {
    const barW = 92;
    const gap = 42;
    const x = 48 + index * (barW + gap);
    const barH = (height - 54) * load / max;
    ctx.fillStyle = "rgba(115,128,150,.11)";
    roundRect2d(ctx, x, 18, barW, height - 44, 14);
    ctx.fill();
    ctx.fillStyle = colorsBar[index];
    roundRect2d(ctx, x, height - 26 - barH, barW, barH, 14);
    ctx.fill();
    ctx.fillStyle = "#172033";
    ctx.font = "700 20px Microsoft YaHei UI";
    ctx.textAlign = "center";
    ctx.fillText(`${load}`, x + barW / 2, height - 4);
    ctx.fillStyle = "#738096";
    ctx.font = "12px Microsoft YaHei UI";
    ctx.fillText(`窗口 ${index + 1}`, x + barW / 2, 14);
  });
}

function drawStateChart(canvas, states) {
  const { ctx, width, height } = prepCanvas(canvas);
  const entries = [
    ["取餐", states.service, "#3b82f6"],
    ["就餐", states.eating, "#10b981"],
    ["等座", states.waiting, "#f59e0b"],
    ["休息", states.resting, "#8b5cf6"],
    ["离开", states.leaving, "#ef4444"],
  ];
  const total = Math.max(1, entries.reduce((sum, item) => sum + item[1], 0));
  let x = 24;
  entries.forEach(([label, value, color]) => {
    const w = (width - 48) * value / total;
    if (w > 0) {
      ctx.fillStyle = color;
      roundRect2d(ctx, x, 26, Math.max(8, w), 30, 12);
      ctx.fill();
    }
    x += w;
  });
  entries.forEach(([label, value, color], index) => {
    const lx = 28 + (index % 3) * 190;
    const ly = 88 + Math.floor(index / 3) * 34;
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(lx, ly - 4, 6, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "#172033";
    ctx.font = "13px Microsoft YaHei UI";
    ctx.fillText(`${label} ${value}`, lx + 14, ly);
  });
}

function animate() {
  const dt = Math.min(clock.getDelta(), 0.04);
  updateSimulation(dt);
  controls.update();
  drawCharts();
  renderer.render(scene, camera);
  requestAnimationFrame(animate);
}

function resize() {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
}

function bindUi() {
  ui.pauseBtn.addEventListener("click", () => {
    sim.paused = !sim.paused;
    ui.pauseBtn.textContent = sim.paused ? "继续" : "暂停";
  });
  ui.resetBtn.addEventListener("click", resetSimulation);
  ui.speedRange.addEventListener("input", () => {
    sim.speed = Number(ui.speedRange.value);
  });
  ui.cameraBtn.addEventListener("click", () => {
    const top = camera.position.y < 55;
    camera.position.set(top ? 0 : 30, top ? 66 : 32, top ? 0.1 : 42);
    controls.target.set(0, 0, 0);
    controls.update();
    ui.cameraBtn.textContent = top ? "透视" : "俯视";
  });
  window.addEventListener("resize", resize);
  window.addEventListener("pointermove", (event) => {
    pointer.x = (event.clientX / window.innerWidth) * 2 - 1;
    pointer.y = -(event.clientY / window.innerHeight) * 2 + 1;
    raycaster.setFromCamera(pointer, camera);
  });
}

buildLighting();
buildCafeteria();
buildHeat();
bindUi();
seedInitialCrowd();
recordMetrics();
animate();
