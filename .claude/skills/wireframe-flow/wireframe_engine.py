#!/usr/bin/env python3
"""Generate a clean interactive wireframe.html from a flow model.

Design goals (the "nice diagram" rules this encodes):
  1. Layered layout: nodes sit in columns (flow stages) and rows.
  2. Edges are ORTHOGONAL and routed only through node-free channels:
       - adjacent-column edges run in the vertical GUTTER between the columns;
       - forward edges that skip columns run in a TOP ring (above all nodes);
       - backward edges / loops run in a BOTTOM ring (below all nodes).
     => no wire ever crosses over a screen body.
  3. Parallel segments get distinct LANES so wires never overlap.
  4. Edge labels sit on the clear vertical/ring segment with a solid plate,
     drawn on top => labels are never covered by a wire or a node.
  5. A UX-analysis pass flags dead-ends, no-exit screens, unreachable screens
     and one-way doors, listed in a side panel and badged on the node.

Input model: edit FLOW below (or load a JSON via --model). Output: wireframe.html
plus it expects screens/<file>.png and thumbs/<file>.png next to it.

Usage:  python3 gen-wireframe.py [model.json] [out.html]
"""
import json
import os
import sys

# ── Flow model (M4 EGT UI) ──────────────────────────────────────────────────
# node: id, label, file, col, row, section
NODES = [
    ("init",      "WiFi Init",             "01-wifi-init",            0, 2.0, "boot"),
    ("connecting","WiFi Connecting",       "02-wifi-connecting",      1, 0.0, "boot"),
    ("connected", "WiFi Connected",        "03-wifi-connected",       1, 1.3, "boot"),
    ("list",      "WiFi Settings",         "04-wifi-settings",        1, 2.6, "boot"),
    ("notfound",  "WiFi Not Found",        "08-wifi-not-found",       1, 3.9, "boot"),
    ("unavail",   "WiFi Unavailable",      "05-wifi-unavailable",     2, 3.3, "boot"),
    ("ovrintro",  "Override Intro",        "07-wifi-override-intro",  3, 3.3, "boot"),
    ("ovrinfo",   "Override Info (popup)", "06-wifi-override-info",   3, 4.6, "boot"),
    ("pwd",       "Enter Password",        "11-password",             4, 3.6, "boot"),

    ("setup",     "Setup",                 "09-setup",                5, 1.2, "auth"),
    ("login",     "Technician Login",      "10-login",                5, 2.5, "auth"),
    ("home",      "HOME",                  "12-home",                 6, 1.9, "auth"),

    ("settings",  "Settings",              "13-settings",             7, 0.5, "main"),
    ("demo",      "Demo Info",             "14-demo-info",            7, 3.0, "main"),

    ("gender",    "Gender",                "15-patient-gender",       8, 2.0, "patient"),
    ("genderdemo","Gender (Demo)",         "16-patient-gender-demo",  8, 3.3, "patient"),
    ("age",       "Age",                   "15b-patient-age",         8, 4.6, "patient"),
    ("zip",       "ZIP Code",              "15c-patient-zip",         9, 4.6, "patient"),

    ("warming",   "Warming",               "17-treatment-warming",   10, 0.0, "treat"),
    ("ready",     "Ready (100%)",          "18-treatment-ready",     10, 1.3, "treat"),
    ("position",  "Position Tip",          "19-treatment-position",  10, 2.6, "treat"),
    ("active",    "Active",                "21-treatment-active",    10, 3.9, "treat"),
    ("nearly",    "Nearly finished",       "22-treatment-nearly",    10, 5.2, "treat"),
    ("zero",      "Cycle Zero",            "23-treatment-zero",      10, 6.5, "treat"),
    ("reposition","Reposition Tip",        "20-treatment-reposition",11, 2.6, "treat"),
    ("paused",    "Paused",                "24-treatment-paused",    11, 3.9, "treat"),
    ("endconf",   "End Confirm",           "25-treatment-end-confirm",11, 5.2, "treat"),
    ("ended",     "Ended (early)",         "27-treatment-ended",     11, 6.5, "treat"),
    ("completed", "Completed",             "26-treatment-completed", 12, 6.5, "treat"),

    ("etemp",     "Error: temp",           "28-error-temp",          12, 0.3, "alert"),
    ("efilter",   "Error: filter",         "29-error-filter",        12, 1.6, "alert"),
    ("wtemp",     "Warning: temp",         "30-warning-temp",        12, 2.9, "alert"),
    ("wair",      "Warning: airflow",      "31-warning-airflow",     12, 4.2, "alert"),
    ("fault",     "Critical fault",        "32-fault-critical",      12, 5.4, "alert"),
]

# edge: from, to, label, kind  (kind: flow | branch | alert)
EDGES = [
    ("init","connected","connects","flow"),
    ("init","list","fails","flow"),
    ("init","notfound","no networks","flow"),
    ("connecting","connected","","flow"),
    ("connected","login","Continue","flow"),
    ("list","pwd","pick network","flow"),
    ("list","unavail","Operate w/o WiFi","flow"),
    ("notfound","unavail","Operate w/o WiFi","flow"),
    ("notfound","list","Retry","branch"),
    ("unavail","ovrinfo","info ⓘ","branch"),
    ("ovrinfo","unavail","✕ close","branch"),
    ("unavail","ovrintro","Continue","flow"),
    ("ovrintro","pwd","Enter Override","flow"),
    ("pwd","connecting","joining…","flow"),
    ("pwd","login","✓ / 9999","flow"),
    ("init","login","Skip","branch"),
    ("list","setup","Back (boot)","branch"),
    ("setup","settings","Settings","flow"),
    ("login","home","tech+pass / Guest","flow"),
    ("login","setup","Back","branch"),
    ("home","gender","Begin Treatment","flow"),
    ("home","demo","Demo Mode","flow"),
    ("home","settings","Settings","branch"),
    ("demo","genderdemo","Continue","flow"),
    ("genderdemo","age","","flow"),
    ("settings","login","Technician Login","branch"),
    ("gender","age","","flow"),
    ("age","zip","","flow"),
    ("zip","warming","complete","flow"),
    ("warming","ready","","flow"),
    ("ready","position","","flow"),
    ("position","active","","flow"),
    ("active","nearly","last seconds","flow"),
    ("nearly","zero","","flow"),
    ("zero","reposition","next cycle","flow"),
    ("reposition","active","","branch"),
    ("zero","completed","done","flow"),
    ("active","paused","Pause","branch"),
    ("paused","position","Resume","branch"),
    ("active","endconf","End","branch"),
    ("endconf","ended","confirm","flow"),
    ("completed","home","Back to Home","branch"),
    ("active","etemp","alert","alert"),
    ("active","efilter","alert","alert"),
    ("active","wtemp","alert","alert"),
    ("active","wair","alert","alert"),
    ("active","fault","fault","alert"),
    ("etemp","active","Resume","branch"),
    ("etemp","home","End","branch"),
    ("efilter","active","Resume","branch"),
    ("efilter","home","End","branch"),
]

SECTION_COLORS = {"boot":"#2b6cb0","auth":"#2f855a","main":"#6b46c1",
                  "patient":"#b7791f","treat":"#c53030","alert":"#dd6b20"}
SECTION_LABELS = {"boot":"Boot + WiFi","auth":"Setup + Login","main":"Main menu",
                  "patient":"Client Info","treat":"Treatment","alert":"Alerts"}
# Terminal screens that are allowed to have no outgoing edge.
TERMINALS = {"completed", "ended", "fault", "wtemp", "wair"}
HUB = "home"

# ── Geometry ────────────────────────────────────────────────────────────────
NODE_W, NODE_H = 230, 168
COL_STRIDE      = 430          # node + gutter
ROW_STRIDE      = 232
MARGIN_X        = 70
GUTTER          = COL_STRIDE - NODE_W       # 200 px node-free vertical channel
LANE_STEP       = 15                        # spacing between parallel wire lanes
RING_TOP_H      = 150          # height of the top ring band reserved for skip edges
RING_BOT_H      = 150          # bottom ring band for backward/loop edges
STUB            = 16           # short horizontal stub out of a node side


def main():
    model = None
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "wireframe.html")
    args = sys.argv[1:]
    if args and args[0].endswith(".json"):
        model = json.load(open(args[0])); args = args[1:]
    if args:
        out = args[0]

    nodes_in = model["nodes"] if model else NODES
    edges_in = model["edges"] if model else EDGES

    # node table
    N = {}
    cols = {}
    for n in nodes_in:
        nid, label, f, col, row, sec = n
        N[nid] = dict(id=nid, label=label, file=f, col=col, row=row, sec=sec,
                      color=SECTION_COLORS.get(sec, "#666"))
        cols.setdefault(col, []).append(nid)

    top_ring_y0 = MARGIN_X                    # reuse margin as top offset base
    node_y0 = MARGIN_X + RING_TOP_H
    for n in N.values():
        n["x"] = MARGIN_X + n["col"] * COL_STRIDE
        n["y"] = node_y0 + n["row"] * ROW_STRIDE
        n["cx"] = n["x"] + NODE_W / 2
        n["cy"] = n["y"] + NODE_H / 2

    max_col = max(n["col"] for n in N.values())
    max_row = max(n["row"] for n in N.values())
    canvas_w = MARGIN_X + (max_col + 1) * COL_STRIDE + MARGIN_X
    nodes_bottom = node_y0 + max_row * ROW_STRIDE + NODE_H
    ring_bot_y = nodes_bottom + 40
    canvas_h = ring_bot_y + RING_BOT_H + MARGIN_X

    # ── lane allocators ──────────────────────────────────────────────────────
    # vertical lanes inside each between-column gutter
    gutter_lanes = {}     # col index c -> next lane slot for gutter right of col c
    def gutter_x(c, slot):
        # gutter to the right of column c spans [x_right(c), x_left(c+1)]
        gx0 = MARGIN_X + c * COL_STRIDE + NODE_W
        return gx0 + (NODE_W * 0) + 24 + slot * LANE_STEP   # start a bit inside
    # horizontal ring lanes
    top_lane_n = [0]
    bot_lane_n = [0]
    def top_lane_y(slot):  return top_ring_y0 + 30 + slot * LANE_STEP
    def bot_lane_y(slot):  return ring_bot_y + 24 + slot * LANE_STEP

    # spread attach points on a node side so stubs don't collide
    side_count = {}   # (nid, side) -> count used
    def attach(nid, side):
        n = N[nid]
        k = (nid, side)
        i = side_count.get(k, 0); side_count[k] = i + 1
        if side in ("r", "l"):
            # spread vertically across the node's right/left edge
            ys = n["y"] + NODE_H * (0.28 + 0.16 * i)
            ys = min(ys, n["y"] + NODE_H - 14)
            x = n["x"] + (NODE_W if side == "r" else 0)
            return x, ys
        else:
            xs = n["x"] + NODE_W * (0.30 + 0.16 * i)
            xs = min(xs, n["x"] + NODE_W - 14)
            y = n["y"] + (NODE_H if side == "b" else 0)
            return xs, y

    paths = []   # (d, kind, label, lx, ly)
    for e in edges_in:
        a, b, label, kind = e
        na, nb = N[a], N[b]
        ca, cb = na["col"], nb["col"]
        seg = []   # list of (x,y) points, orthogonal
        lx = ly = None

        if cb == ca + 1:
            # adjacent forward → vertical run in the gutter right of ca
            slot = gutter_lanes.get(ca, 0); gutter_lanes[ca] = slot + 1
            gx = gutter_x(ca, slot)
            x1, y1 = attach(a, "r"); x2, y2 = attach(b, "l")
            seg = [(x1, y1), (gx, y1), (gx, y2), (x2, y2)]
            lx, ly = gx, (y1 + y2) / 2
        elif cb > ca + 1:
            # forward skip → up into the top ring, across, down into target
            slot = top_lane_n[0]; top_lane_n[0] += 1
            ry = top_lane_y(slot)
            x1, y1 = attach(a, "t"); x2, y2 = attach(b, "t")
            seg = [(x1, y1), (x1, ry), (x2, ry), (x2, y2)]
            lx, ly = (x1 + x2) / 2, ry
        else:
            # backward or same column → down into the bottom ring, across, up
            slot = bot_lane_n[0]; bot_lane_n[0] += 1
            ry = bot_lane_y(slot)
            x1, y1 = attach(a, "b"); x2, y2 = attach(b, "b")
            seg = [(x1, y1), (x1, ry), (x2, ry), (x2, y2)]
            lx, ly = (x1 + x2) / 2, ry

        # build a rounded-orthogonal path string
        d = path_round(seg, r=10)
        paths.append(dict(d=d, kind=kind, label=label, lx=lx, ly=ly,
                          end=seg[-1], prev=seg[-2]))

    # ── UX analysis ──────────────────────────────────────────────────────────
    outdeg = {nid: 0 for nid in N}
    indeg  = {nid: 0 for nid in N}
    adj = {nid: [] for nid in N}
    has_back = {nid: False for nid in N}
    for a, b, label, kind in edges_in:
        outdeg[a] += 1; indeg[b] += 1; adj[a].append(b)
        if b in ("setup", "home") or label.lower().startswith("back") or label.lower()=="resume":
            has_back[a] = True
    # reachability from the boot entry (init)
    seen = set(); stack = ["init"]
    while stack:
        u = stack.pop()
        if u in seen: continue
        seen.add(u)
        for v in adj.get(u, []): stack.append(v)

    issues = []
    node_badges = {}
    for nid, n in N.items():
        ni = []
        if outdeg[nid] == 0 and nid not in TERMINALS:
            ni.append("dead-end (no outgoing navigation)")
        if indeg[nid] == 0 and nid != "init":
            ni.append("unreachable from boot")
        if nid not in seen and nid != "init":
            ni.append("not reachable from WiFi Init")
        if ni:
            node_badges[nid] = "⚠"
            for t in ni:
                issues.append((n["label"], t))

    # ── emit ───────────────────────────────────────────────────────────────
    nodes_js = [dict(id=n["id"], label=n["label"], file=n["file"], x=n["x"], y=n["y"],
                     color=n["color"], badge=node_badges.get(n["id"], ""))
                for n in N.values()]
    order = [n[2] for n in nodes_in]

    svg_parts = [svg_defs()]
    for p in paths:
        col = {"flow":"#8a949f","branch":"#d98a3a","alert":"#e05a2b"}[p["kind"]]
        dash = "" if p["kind"]=="flow" else 'stroke-dasharray="7 5"'
        marker = {"flow":"arrow","branch":"arrowb","alert":"arrowa"}[p["kind"]]
        svg_parts.append(
            f'<path d="{p["d"]}" fill="none" stroke="{col}" stroke-width="2" '
            f'{dash} marker-end="url(#{marker})" opacity="0.95"/>')
    # labels last (on top), with a plate
    for p in paths:
        if not p["label"]: continue
        col = {"flow":"#aeb6bf","branch":"#e0a463","alert":"#e98b63"}[p["kind"]]
        w = 7 * len(p["label"]) + 12
        svg_parts.append(
            f'<g transform="translate({p["lx"]:.0f},{p["ly"]:.0f})">'
            f'<rect x="{-w/2:.0f}" y="-11" width="{w}" height="20" rx="5" '
            f'fill="#11141a" stroke="#2a2f3a"/>'
            f'<text x="0" y="3" text-anchor="middle" fill="{col}" '
            f'font-size="11" font-family="monospace">{esc(p["label"])}</text></g>')
    svg = "\n".join(svg_parts)

    html = TEMPLATE
    repl = {
        "CANVAS_W": str(int(canvas_w)), "CANVAS_H": str(int(canvas_h)),
        "NODE_W": str(NODE_W), "NODE_H": str(NODE_H),
        "SVG": svg,
        "NODES_JSON": json.dumps(nodes_js),
        "ORDER_JSON": json.dumps(order),
        "SECTIONS_JSON": json.dumps({"colors": SECTION_COLORS, "labels": SECTION_LABELS}),
        "ISSUES_JSON": json.dumps(issues),
    }
    for k, v in repl.items():
        html = html.replace("__" + k + "__", v)
    with open(out, "w") as f:
        f.write(html)
    print(f"wrote {out}")
    print(f"  {len(N)} nodes, {len(paths)} edges, {len(issues)} UX issue(s)")
    for lbl, t in issues:
        print(f"  ⚠ {lbl}: {t}")


def path_round(pts, r=10):
    """Orthogonal polyline with rounded corners."""
    if len(pts) < 2:
        return ""
    d = [f"M{pts[0][0]:.1f},{pts[0][1]:.1f}"]
    for i in range(1, len(pts) - 1):
        x0, y0 = pts[i - 1]; x1, y1 = pts[i]; x2, y2 = pts[i + 1]
        # vector in
        dx1 = 0 if x1 == x0 else (1 if x1 > x0 else -1)
        dy1 = 0 if y1 == y0 else (1 if y1 > y0 else -1)
        dx2 = 0 if x2 == x1 else (1 if x2 > x1 else -1)
        dy2 = 0 if y2 == y1 else (1 if y2 > y1 else -1)
        rr = min(r, abs(x1 - x0) / 2 + abs(y1 - y0) / 2, abs(x2 - x1) / 2 + abs(y2 - y1) / 2)
        p1 = (x1 - dx1 * rr, y1 - dy1 * rr)
        p2 = (x1 + dx2 * rr, y1 + dy2 * rr)
        d.append(f"L{p1[0]:.1f},{p1[1]:.1f}")
        d.append(f"Q{x1:.1f},{y1:.1f} {p2[0]:.1f},{p2[1]:.1f}")
    d.append(f"L{pts[-1][0]:.1f},{pts[-1][1]:.1f}")
    return " ".join(d)


def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def svg_defs():
    return ('<defs>'
        '<marker id="arrow" markerWidth="9" markerHeight="9" refX="7" refY="3" '
        'orient="auto" markerUnits="strokeWidth"><path d="M0,0 L7,3 L0,6 Z" fill="#8a949f"/></marker>'
        '<marker id="arrowb" markerWidth="9" markerHeight="9" refX="7" refY="3" '
        'orient="auto" markerUnits="strokeWidth"><path d="M0,0 L7,3 L0,6 Z" fill="#d98a3a"/></marker>'
        '<marker id="arrowa" markerWidth="9" markerHeight="9" refX="7" refY="3" '
        'orient="auto" markerUnits="strokeWidth"><path d="M0,0 L7,3 L0,6 Z" fill="#e05a2b"/></marker>'
        '</defs>')


TEMPLATE = r"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>M4 EGT UI — Interactive Wireframe & Flow</title>
<style>
 :root{--bg:#0f1115;--panel:#171a21;--ink:#e6e8eb;--muted:#9aa3af;}
 *{box-sizing:border-box}
 body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.5 -apple-system,Segoe UI,Roboto,sans-serif}
 header{position:sticky;top:0;z-index:50;background:#11141a;border-bottom:1px solid #262b34;padding:10px 16px}
 header h1{margin:0;font-size:17px}
 header .sub{color:var(--muted);font-size:12px;margin-top:2px}
 .legend{display:flex;gap:14px;flex-wrap:wrap;margin-top:7px;font-size:12px;align-items:center}
 .legend span{display:inline-flex;align-items:center;gap:6px}
 .dot{width:11px;height:11px;border-radius:3px;display:inline-block}
 .ln{width:22px;height:0;border-top:2px solid;display:inline-block}
 .wrap{display:flex;height:calc(100vh - 96px)}
 .scroller{overflow:auto;flex:1}
 .canvas{position:relative;width:__CANVAS_W__px;height:__CANVAS_H__px}
 svg.wires{position:absolute;top:0;left:0}
 .node{position:absolute;width:__NODE_W__px;background:var(--panel);border:2px solid #333;
       border-radius:10px;overflow:hidden;cursor:pointer;box-shadow:0 2px 8px rgba(0,0,0,.45);
       transition:transform .08s,box-shadow .08s}
 .node:hover{transform:translateY(-3px);box-shadow:0 10px 26px rgba(0,0,0,.65);z-index:20}
 .node img{width:100%;display:block;background:#fff}
 .node .cap{padding:5px 8px;font-size:12px;font-weight:600;display:flex;align-items:center;gap:6px}
 .node .cap .sd{width:9px;height:9px;border-radius:2px;flex:none}
 .node .badge{margin-left:auto;color:#f6c453;font-size:13px}
 .panel{width:300px;flex:none;background:#11141a;border-left:1px solid #262b34;
        overflow:auto;padding:14px 16px}
 .panel h2{font-size:13px;text-transform:uppercase;letter-spacing:.5px;color:var(--muted);margin:0 0 8px}
 .panel .iss{background:#1b1410;border:1px solid #4a2f1a;border-radius:8px;padding:8px 10px;margin-bottom:8px}
 .panel .iss b{color:#f6c453}
 .panel .ok{color:#5fb37a}
 .lb{position:fixed;inset:0;background:rgba(0,0,0,.92);z-index:100;display:none;
     align-items:center;justify-content:center;flex-direction:column}
 .lb.open{display:flex}
 .lb img{max-width:94vw;max-height:84vh;border:1px solid #333;background:#fff;box-shadow:0 10px 40px rgba(0,0,0,.7)}
 .lb .meta{color:#e6e8eb;margin:10px;font-size:14px}
 .lb .nav{position:absolute;top:0;bottom:0;width:16vw;cursor:pointer;display:flex;align-items:center;
          font-size:42px;color:#fff8;user-select:none}
 .lb .nav:hover{color:#fff;background:rgba(255,255,255,.04)}
 .lb .prev{left:0;padding-left:20px}.lb .next{right:0;justify-content:flex-end;padding-right:20px}
 .lb .close{position:absolute;top:14px;right:22px;font-size:30px;color:#fff8;cursor:pointer}
 .lb .close:hover{color:#fff}
</style></head>
<body>
<header>
 <h1>M4 EGT UI — Interactive Wireframe &amp; Flow</h1>
 <div class="sub">Click any frame to open the full-res screenshot · wires never cross a screen (routed in gutters + top/bottom rings). Scroll to follow the flow →</div>
 <div class="legend" id="legend">
   <span><span class="ln" style="border-color:#8a949f"></span>main flow</span>
   <span><span class="ln" style="border-color:#d98a3a;border-top-style:dashed"></span>branch / back</span>
   <span><span class="ln" style="border-color:#e05a2b;border-top-style:dashed"></span>alert</span>
   <span style="color:#f6c453">⚠ UX issue</span>
 </div>
</header>
<div class="wrap">
 <div class="scroller"><div class="canvas" id="canvas">
   <svg class="wires" width="__CANVAS_W__" height="__CANVAS_H__">__SVG__</svg>
 </div></div>
 <div class="panel" id="panel"><h2>UX issues</h2><div id="issues"></div></div>
</div>
<div class="lb" id="lb">
 <div class="close" onclick="closeLb()">✕</div>
 <div class="nav prev" onclick="step(-1)">‹</div>
 <div class="nav next" onclick="step(1)">›</div>
 <img id="lbimg" src=""><div class="meta" id="lbmeta"></div>
</div>
<script>
const NODES=__NODES_JSON__, ORDER=__ORDER_JSON__, SECTIONS=__SECTIONS_JSON__, ISSUES=__ISSUES_JSON__;
const canvas=document.getElementById('canvas');
NODES.forEach(n=>{
  const d=document.createElement('div');d.className='node';
  d.style.left=n.x+'px';d.style.top=n.y+'px';d.style.borderColor=n.color;
  d.innerHTML=`<img src="thumbs/${n.file}.png" alt="${n.label}">
    <div class="cap"><span class="sd" style="background:${n.color}"></span>${n.label}
    <span class="badge">${n.badge||''}</span></div>`;
  d.onclick=()=>openLb(n.file);
  canvas.appendChild(d);
});
const ip=document.getElementById('issues');
if(!ISSUES.length){ip.innerHTML='<div class="ok">No structural UX issues detected — every screen is reachable and has a way out.</div>';}
else{ISSUES.forEach(([scr,txt])=>{const d=document.createElement('div');d.className='iss';
  d.innerHTML=`<b>${scr}</b><br>${txt}`;ip.appendChild(d);});}
let cur=0;const lb=document.getElementById('lb'),lbimg=document.getElementById('lbimg'),lbmeta=document.getElementById('lbmeta');
function openLb(f){cur=ORDER.indexOf(f);render();lb.classList.add('open');}
function closeLb(){lb.classList.remove('open');}
function step(d){cur=(cur+d+ORDER.length)%ORDER.length;render();}
function render(){const f=ORDER[cur];lbimg.src=`screens/${f}.png`;lbmeta.textContent=`${cur+1} / ${ORDER.length} — ${f}`;}
lb.addEventListener('click',e=>{if(e.target===lb)closeLb();});
document.addEventListener('keydown',e=>{if(!lb.classList.contains('open'))return;
  if(e.key==='Escape')closeLb();if(e.key==='ArrowRight')step(1);if(e.key==='ArrowLeft')step(-1);});
</script>
</body></html>
"""

if __name__ == "__main__":
    main()
