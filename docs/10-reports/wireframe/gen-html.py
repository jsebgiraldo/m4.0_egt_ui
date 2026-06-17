#!/usr/bin/env python3
"""Generate an interactive wireframe.html: screen thumbnails laid out by flow
stage, connected with SVG wires, each clickable to open the full-res PNG in a
lightbox. Self-contained (no external JS/CDN); references local screens/ + thumbs/.

Run from docs/10-reports/wireframe/:  python3 gen-html.py
"""
import json
import os

# Node: id, label, file basename (matches screens/<file>.png and thumbs/<file>.png),
# col (flow stage 0..8), row (position within the stage), and section.
NODES = [
    # id,                 label,                  file,                       col, row, section
    ("init",      "WiFi Init",            "01-wifi-init",            0, 2.0, "boot"),
    ("connecting","WiFi Connecting",      "02-wifi-connecting",      1, 0.2, "boot"),
    ("connected", "WiFi Connected",       "03-wifi-connected",       1, 1.4, "boot"),
    ("list",      "WiFi Settings",        "04-wifi-settings",        1, 2.6, "boot"),
    ("notfound",  "WiFi Not Found",       "08-wifi-not-found",       1, 3.8, "boot"),
    ("unavail",   "WiFi Unavailable",     "05-wifi-unavailable",     2, 3.2, "boot"),
    ("ovrinfo",   "Override Info (popup)","06-wifi-override-info",   3, 2.4, "boot"),
    ("ovrintro",  "Override Intro",       "07-wifi-override-intro",  2, 4.4, "boot"),
    ("pwd",       "Enter Password",       "11-password",             3, 3.6, "boot"),

    ("setup",     "Setup",                "09-setup",                4, 1.2, "auth"),
    ("login",     "Technician Login",     "10-login",                4, 2.4, "auth"),
    ("home",      "HOME",                 "12-home",                 5, 1.8, "auth"),

    ("settings",  "Settings",             "13-settings",             6, 0.6, "main"),
    ("demo",      "Demo Info",            "14-demo-info",            6, 2.8, "main"),

    ("gender",    "Gender",               "15-patient-gender",       7, 2.0, "patient"),
    ("genderdemo","Gender (Demo)",        "16-patient-gender-demo",  7, 0.8, "patient"),
    ("age",       "Age",                  "15b-patient-age",         7, 3.2, "patient"),
    ("zip",       "ZIP Code",             "15c-patient-zip",         7, 4.4, "patient"),

    ("warming",   "Warming",              "17-treatment-warming",    8, 0.0, "treat"),
    ("ready",     "Ready (100%)",         "18-treatment-ready",      8, 1.1, "treat"),
    ("position",  "Position Tip",         "19-treatment-position",   8, 2.2, "treat"),
    ("reposition","Reposition Tip",       "20-treatment-reposition", 9, 2.2, "treat"),
    ("active",    "Active",               "21-treatment-active",     8, 3.3, "treat"),
    ("nearly",    "Nearly finished",      "22-treatment-nearly",     8, 4.4, "treat"),
    ("zero",      "Cycle Zero",           "23-treatment-zero",       8, 5.5, "treat"),
    ("paused",    "Paused",               "24-treatment-paused",     9, 3.3, "treat"),
    ("endconf",   "End Confirm",          "25-treatment-end-confirm",9, 4.4, "treat"),
    ("completed", "Completed",            "26-treatment-completed",  8, 6.6, "treat"),
    ("ended",     "Ended (early)",        "27-treatment-ended",      9, 5.5, "treat"),

    ("etemp",     "Error: temp",          "28-error-temp",          10, 0.6, "alert"),
    ("efilter",   "Error: filter",        "29-error-filter",        10, 1.7, "alert"),
    ("wtemp",     "Warning: temp",        "30-warning-temp",        10, 2.8, "alert"),
    ("wair",      "Warning: airflow",     "31-warning-airflow",     10, 3.9, "alert"),
    ("fault",     "Critical fault",       "32-fault-critical",      10, 5.0, "alert"),
]

# Edges: from, to, label, kind ("solid" normal flow, "dash" alert/branch)
EDGES = [
    ("init","connected","connects","solid"),
    ("init","list","fails","solid"),
    ("init","notfound","no networks","solid"),
    ("init","login","Skip","solid"),
    ("connecting","connected","","solid"),
    ("connected","login","Continue","solid"),
    ("list","pwd","pick network","solid"),
    ("pwd","connected","network OK","solid"),
    ("list","unavail","Operate w/o WiFi","solid"),
    ("list","settings","gear","solid"),
    ("notfound","unavail","Operate w/o WiFi","solid"),
    ("notfound","list","Retry","solid"),
    ("notfound","settings","Setting","solid"),
    ("unavail","ovrinfo","info","dash"),
    ("unavail","ovrintro","Continue","solid"),
    ("ovrintro","pwd","Enter Override","solid"),
    ("pwd","login",'"9999"',"solid"),
    ("list","setup","Back (boot)","dash"),
    ("setup","settings","Settings","solid"),
    ("login","home","tech+pass / Guest","solid"),
    ("login","setup","Back","dash"),
    ("home","gender","Begin Treatment","solid"),
    ("home","demo","Demo Mode","solid"),
    ("home","settings","Settings","solid"),
    ("demo","gender","Continue","solid"),
    ("settings","list","WiFi","dash"),
    ("settings","login","Technician Login","dash"),
    ("gender","age","","solid"),
    ("age","zip","","solid"),
    ("zip","warming","complete","solid"),
    ("warming","ready","","solid"),
    ("ready","position","","solid"),
    ("position","active","","solid"),
    ("active","nearly","last seconds","solid"),
    ("nearly","zero","","solid"),
    ("zero","reposition","another cycle","solid"),
    ("reposition","active","","solid"),
    ("zero","completed","done","solid"),
    ("active","paused","Pause","dash"),
    ("paused","position","Resume","dash"),
    ("active","endconf","End","dash"),
    ("endconf","ended","confirm","solid"),
    ("completed","home","Back to Home","dash"),
    ("ended","home","auto 20s","dash"),
    ("active","etemp","alert","dash"),
    ("active","efilter","alert","dash"),
    ("active","wtemp","alert","dash"),
    ("active","wair","alert","dash"),
    ("active","fault","fault","dash"),
]

SECTION_COLORS = {
    "boot":   "#2b6cb0",
    "auth":   "#2f855a",
    "main":   "#6b46c1",
    "patient":"#b7791f",
    "treat":  "#c53030",
    "alert":  "#dd6b20",
}
SECTION_LABELS = {
    "boot": "Boot + WiFi", "auth": "Setup + Login", "main": "Main menu",
    "patient": "Client Info", "treat": "Treatment", "alert": "Alerts",
}

# Layout geometry
COL_W = 330      # horizontal stride per flow stage
ROW_H = 250      # vertical stride per row unit
NODE_W = 280
NODE_H = 196
PAD_X = 40
PAD_Y = 80

def node_xy(n):
    _id, _lbl, _f, col, row, _sec = n
    x = PAD_X + col * COL_W
    y = PAD_Y + row * ROW_H
    return x, y

nodes_js = []
for n in NODES:
    _id, lbl, f, col, row, sec = n
    x, y = node_xy(n)
    nodes_js.append({"id": _id, "label": lbl, "file": f, "x": x, "y": y,
                     "sec": sec, "color": SECTION_COLORS[sec]})

edges_js = [{"from": e[0], "to": e[1], "label": e[2], "kind": e[3]} for e in EDGES]

max_x = max(nd["x"] for nd in nodes_js) + NODE_W + PAD_X
max_y = max(nd["y"] for nd in nodes_js) + NODE_H + PAD_Y

ordered_files = [n[2] for n in NODES]

HTML = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>M4 EGT UI — Interactive Wireframe & Flow</title>
<style>
  :root { --bg:#0f1115; --panel:#171a21; --ink:#e6e8eb; --muted:#9aa3af; }
  * { box-sizing: border-box; }
  body { margin:0; background:var(--bg); color:var(--ink);
         font:14px/1.5 -apple-system,Segoe UI,Roboto,sans-serif; }
  header { position:sticky; top:0; z-index:50; background:#11141a;
           border-bottom:1px solid #262b34; padding:12px 18px; }
  header h1 { margin:0; font-size:18px; }
  header .sub { color:var(--muted); font-size:12px; margin-top:2px; }
  .legend { display:flex; gap:14px; flex-wrap:wrap; margin-top:8px; font-size:12px; }
  .legend span { display:inline-flex; align-items:center; gap:6px; }
  .dot { width:11px; height:11px; border-radius:3px; display:inline-block; }
  .scroller { overflow:auto; height:calc(100vh - 92px); }
  .canvas { position:relative; }
  svg.wires { position:absolute; top:0; left:0; pointer-events:none; }
  .node { position:absolute; width:__NODE_W__px; background:var(--panel);
          border:2px solid #333; border-radius:10px; overflow:hidden;
          cursor:pointer; transition:transform .08s, box-shadow .08s;
          box-shadow:0 2px 8px rgba(0,0,0,.4); }
  .node:hover { transform:translateY(-3px); box-shadow:0 8px 22px rgba(0,0,0,.6); z-index:10; }
  .node img { width:100%; display:block; background:#fff; }
  .node .cap { padding:6px 8px; font-size:12px; font-weight:600;
               display:flex; align-items:center; gap:6px; }
  .node .cap .sd { width:9px; height:9px; border-radius:2px; flex:none; }
  .elabel { fill:#cbd5e0; font-size:11px; paint-order:stroke;
            stroke:#0f1115; stroke-width:4px; }
  /* lightbox */
  .lb { position:fixed; inset:0; background:rgba(0,0,0,.92); z-index:100;
        display:none; align-items:center; justify-content:center; flex-direction:column; }
  .lb.open { display:flex; }
  .lb img { max-width:94vw; max-height:84vh; border:1px solid #333;
            box-shadow:0 10px 40px rgba(0,0,0,.7); background:#fff; }
  .lb .meta { color:#e6e8eb; margin:10px; font-size:14px; }
  .lb .nav { position:absolute; top:0; bottom:0; width:18vw; cursor:pointer;
             display:flex; align-items:center; font-size:40px; color:#fff8;
             user-select:none; }
  .lb .nav:hover { color:#fff; background:rgba(255,255,255,.04); }
  .lb .prev { left:0; justify-content:flex-start; padding-left:20px; }
  .lb .next { right:0; justify-content:flex-end; padding-right:20px; }
  .lb .close { position:absolute; top:14px; right:22px; font-size:30px;
               color:#fff8; cursor:pointer; }
  .lb .close:hover { color:#fff; }
  .hint { color:var(--muted); font-size:12px; }
</style>
</head>
<body>
<header>
  <h1>M4 EGT UI — Interactive Wireframe &amp; Flow</h1>
  <div class="sub">Click any frame to open the full-resolution screenshot · arrows/wires follow the real navigation. <span class="hint">Scroll horizontally to follow the flow →</span></div>
  <div class="legend" id="legend"></div>
</header>
<div class="scroller">
  <div class="canvas" id="canvas" style="width:__MAXX__px; height:__MAXY__px;">
    <svg class="wires" id="wires" width="__MAXX__" height="__MAXY__"></svg>
  </div>
</div>

<div class="lb" id="lb">
  <div class="close" onclick="closeLb()">✕</div>
  <div class="nav prev" onclick="step(-1)">‹</div>
  <div class="nav next" onclick="step(1)">›</div>
  <img id="lbimg" src="">
  <div class="meta" id="lbmeta"></div>
</div>

<script>
const NODES = __NODES_JSON__;
const EDGES = __EDGES_JSON__;
const ORDER = __ORDER_JSON__;
const SECTIONS = __SECTIONS_JSON__;
const NW = __NODE_W__, NH = __NODE_H__;

// legend
const lg = document.getElementById('legend');
for (const [key, col] of Object.entries(SECTIONS.colors)) {
  const s = document.createElement('span');
  s.innerHTML = `<span class="dot" style="background:${col}"></span>${SECTIONS.labels[key]}`;
  lg.appendChild(s);
}

const canvas = document.getElementById('canvas');
const byId = {};
NODES.forEach(n => byId[n.id] = n);

// place nodes
NODES.forEach(n => {
  const d = document.createElement('div');
  d.className = 'node';
  d.style.left = n.x + 'px';
  d.style.top  = n.y + 'px';
  d.style.borderColor = n.color;
  d.innerHTML = `<img src="thumbs/${n.file}.png" alt="${n.label}">
                 <div class="cap"><span class="sd" style="background:${n.color}"></span>${n.label}</div>`;
  d.onclick = () => openLb(n.file);
  canvas.appendChild(d);
});

// draw wires
const svg = document.getElementById('wires');
const SVGNS = 'http://www.w3.org/2000/svg';
// arrow marker
svg.innerHTML = `<defs>
  <marker id="arrow" markerWidth="9" markerHeight="9" refX="7" refY="3"
          orient="auto" markerUnits="strokeWidth">
    <path d="M0,0 L7,3 L0,6 Z" fill="#7c8694"/>
  </marker>
  <marker id="arrowd" markerWidth="9" markerHeight="9" refX="7" refY="3"
          orient="auto" markerUnits="strokeWidth">
    <path d="M0,0 L7,3 L0,6 Z" fill="#dd6b20"/>
  </marker>
</defs>`;

function anchor(n, side){
  // return a point on node n's edge: 'r' right-mid, 'l' left-mid, 't','b'
  const cx = n.x + NW/2, cy = n.y + NH/2;
  if (side==='r') return [n.x+NW, cy];
  if (side==='l') return [n.x, cy];
  if (side==='t') return [cx, n.y];
  if (side==='b') return [cx, n.y+NH];
  return [cx, cy];
}

EDGES.forEach(e => {
  const a = byId[e.from], b = byId[e.to];
  if (!a || !b) return;
  // pick sides: prefer horizontal flow (a right -> b left) when b is to the right
  let sa, sb;
  if (b.x > a.x + 20) { sa='r'; sb='l'; }
  else if (b.x < a.x - 20) { sa='l'; sb='r'; }
  else if (b.y > a.y) { sa='b'; sb='t'; }
  else { sa='t'; sb='b'; }
  const [x1,y1] = anchor(a, sa);
  const [x2,y2] = anchor(b, sb);
  // bezier control points
  const dx = Math.abs(x2-x1), k = Math.max(40, dx*0.4);
  let c1x=x1, c1y=y1, c2x=x2, c2y=y2;
  if (sa==='r'){c1x=x1+k;} if(sa==='l'){c1x=x1-k;}
  if (sa==='b'){c1y=y1+k;} if(sa==='t'){c1y=y1-k;}
  if (sb==='l'){c2x=x2-k;} if(sb==='r'){c2x=x2+k;}
  if (sb==='t'){c2y=y2-k;} if(sb==='b'){c2y=y2+k;}
  const p = document.createElementNS(SVGNS,'path');
  p.setAttribute('d', `M${x1},${y1} C${c1x},${c1y} ${c2x},${c2y} ${x2},${y2}`);
  p.setAttribute('fill','none');
  const dash = e.kind==='dash';
  p.setAttribute('stroke', dash ? '#dd6b20' : '#7c8694');
  p.setAttribute('stroke-width', dash ? '1.6' : '2');
  if (dash) p.setAttribute('stroke-dasharray','6 5');
  p.setAttribute('marker-end', dash ? 'url(#arrowd)' : 'url(#arrow)');
  p.setAttribute('opacity', dash ? '0.85' : '1');
  svg.appendChild(p);
  if (e.label){
    const t = document.createElementNS(SVGNS,'text');
    t.setAttribute('class','elabel');
    t.setAttribute('x', (x1+x2)/2);
    t.setAttribute('y', (y1+y2)/2 - 4);
    t.setAttribute('text-anchor','middle');
    t.textContent = e.label;
    svg.appendChild(t);
  }
});

// lightbox
let cur = 0;
const lb = document.getElementById('lb');
const lbimg = document.getElementById('lbimg');
const lbmeta = document.getElementById('lbmeta');
function openLb(file){ cur = ORDER.indexOf(file); render(); lb.classList.add('open'); }
function closeLb(){ lb.classList.remove('open'); }
function step(d){ cur = (cur + d + ORDER.length) % ORDER.length; render(); }
function render(){
  const file = ORDER[cur];
  lbimg.src = `screens/${file}.png`;
  lbmeta.textContent = `${cur+1} / ${ORDER.length} — ${file}`;
}
lb.addEventListener('click', e => { if (e.target === lb) closeLb(); });
document.addEventListener('keydown', e => {
  if (!lb.classList.contains('open')) return;
  if (e.key==='Escape') closeLb();
  if (e.key==='ArrowRight') step(1);
  if (e.key==='ArrowLeft') step(-1);
});
</script>
</body>
</html>
"""
_subs = {
    "NODE_W": str(NODE_W), "NODE_H": str(NODE_H),
    "MAXX": str(max_x), "MAXY": str(max_y),
    "NODES_JSON": json.dumps(nodes_js),
    "EDGES_JSON": json.dumps(edges_js),
    "ORDER_JSON": json.dumps(ordered_files),
    "SECTIONS_JSON": json.dumps({"colors": SECTION_COLORS, "labels": SECTION_LABELS}),
}
for _k, _v in _subs.items():
    HTML = HTML.replace("__" + _k + "__", _v)

out = os.path.join(os.path.dirname(__file__), "wireframe.html")
with open(out, "w") as f:
    f.write(HTML)
print(f"wrote {out} ({len(NODES)} nodes, {len(EDGES)} edges)")
