# LED Effects Module — Quick Guide

This module defines a stable ABI for effects. An effect is a pair of functions `(init, render)` registered in a table. The engine calls `render` each tick with channel state, params, and absolute time.

## Adding a new effect
1. Create functions `fx_<name>_init` and `fx_<name>_render` in `effects.c`.
2. Add an entry to `EFFECTS[]` with a new `id` and name.
3. Reference the effect in presets by `effect_id` or name → id mapping.

## Render contract
- Write color values into `ch->framebuf[p]` (RGB or RGBW).
- Respect `ch->max_brightness` and any global power caps.
- Avoid dynamic allocations; keep per-effect state in static or channel-local scratch (indexed by `ch`).

See `effects.c` for Solid, Gradient, Chase, Twinkle examples.
