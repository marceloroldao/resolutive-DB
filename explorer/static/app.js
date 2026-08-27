const NS = "http://www.w3.org/2000/svg";

const $ = (selector) => document.querySelector(selector);

function svg(name, attrs = {}) {
  const el = document.createElementNS(NS, name);
  for (const [key, value] of Object.entries(attrs)) el.setAttribute(key, value);
  return el;
}

function metric(label, value) {
  const el = document.createElement("div");
  el.className = "metric";
  el.innerHTML = `<span class="label">${label}</span><span class="value">${value}</span>`;
  return el;
}

function renderMetrics(stats) {
  const root = $("#metrics");
  root.replaceChildren();
  const entries = [
    ["registros", stats.records ?? 0],
    ["buckets ocupados", stats.occupied_buckets ?? 0],
    ["fator de carga", Number(stats.load_factor ?? 0).toFixed(5)],
    ["slots de fase", stats.phase_slots ?? 0],
    ["colisão exata máx.", stats.max_exact_collisions ?? 0],
  ];
  for (const item of entries) root.appendChild(metric(...item));
}

function showNode(node, group) {
  document.querySelectorAll(".node.active").forEach((item) => item.classList.remove("active"));
  group.classList.add("active");
  $("#inspector-empty").hidden = true;
  const panel = $("#inspector");
  panel.hidden = false;
  const fields = [
    ["ID", node.id],
    ["ρR", node.rho_R],
    ["φ", node.phi],
    ["θ", Number(node.theta).toFixed(8)],
    ["fν", Number(node.f_nu).toFixed(8)],
    ["payload bytes", node.payload_size],
    ["fingerprint", node.fingerprint],
    ["payload", node.payload_preview, "payload"],
  ];
  panel.replaceChildren();
  for (const [label, value, className] of fields) {
    const dt = document.createElement("dt");
    dt.textContent = label;
    const dd = document.createElement("dd");
    if (className) dd.className = className;
    dd.textContent = String(value);
    panel.append(dt, dd);
  }
}

function renderMap(nodes, stats) {
  const map = $("#map");
  map.replaceChildren();
  const cx = 450;
  const cy = 300;
  const maxRadius = 250;
  const bucketCount = Math.max(Number(stats.bucket_count ?? 1), 1);

  for (const r of [65, 125, 185, 250]) {
    map.appendChild(svg("circle", { cx, cy, r, class: "orbit" }));
  }
  map.appendChild(svg("line", { x1: 120, y1: cy, x2: 780, y2: cy, class: "axis" }));
  map.appendChild(svg("line", { x1: cx, y1: 35, x2: cx, y2: 565, class: "axis" }));
  map.appendChild(svg("circle", { cx, cy, r: 5, fill: "rgba(167,243,208,.9)" }));

  for (const node of nodes) {
    const normalizedRho = Math.min(Math.max(Number(node.rho_R) / Math.max(bucketCount - 1, 1), 0), 1);
    const radius = 24 + normalizedRho * (maxRadius - 24);
    const angle = Number(node.theta) - Math.PI / 2;
    const x = cx + Math.cos(angle) * radius;
    const y = cy + Math.sin(angle) * radius;
    const size = 5 + Math.min(Math.max(Number(node.f_nu), 0), 1) * 7;
    const group = svg("g", { class: "node", tabindex: "0", "aria-label": `Nódulo ${node.id}` });
    group.appendChild(svg("circle", { cx: x.toFixed(2), cy: y.toFixed(2), r: size.toFixed(2) }));
    group.addEventListener("click", () => showNode(node, group));
    group.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") showNode(node, group);
    });
    map.appendChild(group);
  }
}

async function start() {
  try {
    const health = await fetch("/api/health", { cache: "no-store" }).then((r) => r.json());
    $("#health").textContent = health.status === "ok" ? "BDR conectado · somente leitura" : "estado desconhecido";
    const snapshot = await fetch("/api/snapshot", { cache: "no-store" }).then((r) => r.json());
    $("#version").textContent = `BDR ${snapshot.bdr_version}`;
    renderMetrics(snapshot.statistics);
    renderMap(snapshot.nodes, snapshot.statistics);
  } catch (error) {
    $("#health").textContent = "falha de conexão";
    console.error(error);
  }
}

start();
