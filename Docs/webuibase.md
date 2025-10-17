Lumi Grid Web Ui — Single‑file Esp Build (index
· html
<!doctype html>
<html lang="en" data-theme="dark">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>LumiGrid — LED Node UI</title>
  <meta name="color-scheme" content="dark light" />
  <style>
    /* ------------------------------
       Design Tokens (CSS Variables)
       ------------------------------ */
    :root{
      --bg: #0f1115;
      --surface: #151924;
      --muted: #1c2232;
      --text: #e6eaf2;
      --subtext:#9BA7C0;
      --primary:#6C7CFF;
      --primary-600:#5366ff;
      --primary-700:#4256f3;
      --accent:#86E2FF;
      --danger:#ff5c7a;
      --warn:#ffc857;
      --success:#38e1b3;
      --focus:#86E2FF;
      --ring: 0 0 0 .16rem var(--focus), 0 0 0 .28rem color-mix(in oklab, var(--focus) 30%, transparent);
      --radius: 14px;
      --radius-sm: 10px;
      --shadow-md: 0 10px 30px rgba(0,0,0,.35);
      --shadow-sm: 0 2px 10px rgba(0,0,0,.25);
      --gap: 14px;
    }
    [data-theme="light"]{
      --bg:#f7f8fb; --surface:#ffffff; --muted:#eef1f7; --text:#0e1220; --subtext:#3b4866;
    }
    *{box-sizing:border-box}
    html,body{height:100%}
    body{
      margin:0; background:var(--bg); color:var(--text); font: 15px/1.5 system-ui, Segoe UI, Roboto, Ubuntu, Cantarell, Noto Sans, Helvetica, Arial, Apple Color Emoji, Segoe UI Emoji;
      -webkit-font-smoothing:antialiased; text-rendering:optimizeLegibility;
    }
    a{color:var(--accent); text-decoration:none}
    a:focus-visible{outline:none; box-shadow:var(--ring); border-radius:8px}


    /* Layout */
    .app{display:grid; grid-template-rows: 56px 1fr; height:100%}
    .topbar{display:flex; align-items:center; gap:10px; padding:0 14px; border-bottom:1px solid #20263a; background:linear-gradient(180deg,var(--surface), color-mix(in oklab, var(--surface) 80%, transparent));}
    .brand{display:flex; align-items:center; gap:10px; font-weight:700}
    .brand svg{width:24px; height:24px}
    .top-actions{margin-left:auto; display:flex; gap:10px; align-items:center}
    .pill{display:inline-flex; align-items:center; gap:8px; padding:6px 10px; border-radius:999px; background:var(--muted); color:var(--subtext)}
    .status-dot{width:8px; height:8px; border-radius:50%; background:var(--success); box-shadow:0 0 10px var(--success)}


    .main{display:grid; grid-template-columns: 220px 1fr; height:100%}
    nav{border-right:1px solid #20263a; background:linear-gradient(180deg,var(--surface), color-mix(in oklab, var(--surface) 85%, transparent));}
    .navlink{display:flex; align-items:center; gap:10px; padding:10px 14px; color:var(--subtext); border-left:3px solid transparent}
    .navlink:hover{background:color-mix(in oklab, var(--muted) 80%, transparent); color:var(--text)}
    .navlink[aria-current="page"]{color:var(--text); background:var(--muted); border-left-color:var(--primary)}
    .content{padding:16px; overflow:auto}


    /* Controls */
    .btn{display:inline-flex; align-items:center; gap:8px; padding:10px 14px; border-radius:12px; background:var(--primary); color:white; border:none; cursor:pointer; box-shadow:var(--shadow-sm)}
    .btn:focus-visible{outline:none; box-shadow:var(--ring)}
    .btn.ghost{background:transparent; border:1px solid #2a3248; color:var(--text)}
    .btn.danger{background:var(--danger)}
    .btn.small{padding:6px 10px; border-radius:10px}
    .seg{display:inline-flex; border:1px solid #2a3248; border-radius:12px; overflow:hidden}
    .seg button{padding:8px 12px; background:transparent; color:var(--subtext); border:none}
    .seg button[aria-pressed="true"]{background:var(--primary); color:white}
    .input, select{width:100%; background:var(--muted); color:var(--text); border:1px solid #2a3248; border-radius:10px; padding:10px 12px}
    .input:focus-visible, select:focus-visible{outline:none; box-shadow:var(--ring)}
    label{display:block; margin:0 0 6px; color:var(--subtext)}



Dropped a complete, single-file Web UI into the canvas: modern, accessible, and ESP-friendly (no frameworks, responsive, keyboard-navigable, contrast-checked). It includes:

App shell with nav (Dashboard, Nodes, Sequencer, Presets, Schedules, Settings)

Design tokens, dark/light themes, focus rings, reduced motion ready

Cards, tables, buttons, badges—styled and reusable

Fully working Canvas sequencer (pan/zoom, playhead, add clips, time grid)

REST helpers wired to your endpoint shapes (/api/status, /api/config, /api/presets, /api/trigger, /api/cue)

Accessible roles/ARIA, keyboard focus, and high-contrast defaults

How to deploy on the ESP (static hosting)

Save the file as index.html.

Gzip it (smaller + faster):

gzip -9k index.html   # produces index.html.gz


Put it in your LittleFS/SPIFFS web root (e.g., /spiffs/www/index.html.gz).

Make sure your static handler sets:

Content-Type: text/html; charset=utf-8
Content-Encoding: gzip
Cache-Control: public, max-age=31536000, immutable (for fingerprinted assets)