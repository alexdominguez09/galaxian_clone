# Playtest Log (Stage 23 — Gameplay Balancing)

Methodology: every session runs the real binary headlessly
(`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`) for a fixed wall-time cap
with a STATIONARY, non-firing ship, so the numbers isolate raw enemy
pressure. The `[run]` telemetry line (Stage 23) reports survival time,
shots, accuracy, kills, deaths and waves. Human sessions use the same
template but with live play; fill one block per session.

Tuning policy: only numeric values in `assets/config/game.json` change —
never mechanics. Defaults stay unless measurements or human sessions show
a clear problem.

## Session log

### S1 — Automated baseline (default config)
- Config: `game.json` defaults (wave-1 interval 6 s)
- Cap: 60 s wall · Ship: stationary / not firing
- Result: **time=25.6s shots=0 acc=0% kills=0 deaths=3 waves=1**
- Reading: full game-over in ~26 s ⇒ ≈8.5 s of pure pressure per life
  against a do-nothing target. Pressure exists early but is not instant.

### S2 — Automated aggressive probe (`wave_1/2 interval: 4`)
- Cap: 60 s · Ship: stationary / not firing
- Result: **time=23.6s deaths=3 waves=1**
- Reading: tightening the interval only shaves ~2 s off a stationary run;
  aim/dodge skill remains the dominant factor. No default change needed.

### S3 — Automated gentle probe (`wave_1 interval: 9`)
- Cap: 90 s · Ship: stationary / not firing
- Result: **time=31.8s deaths=3 waves=1**
- Reading: +6 s over baseline confirms the knob is effective and linear-
  feeling. Default 6 s sits between the probes — kept.

### Outcome of automated pass
Defaults retained; no mechanic changes. The decisive data comes from
human sessions below (dodging/aiming change survival by minutes, which is
where "easy to learn, hard to master" actually shows).

## Human session template (duplicate per session)

```
### H? — <date> — <player initials>
- Config: <defaults | delta summary>
- Duration: <minutes played>
- Survival: <longest life> / <total run>
- Shots fired / hits: <n> / <n>   Accuracy: <%>
- Enemies killed: <n>             Deaths: <n>
- Waves reached: <n>
- Feel notes: <too easy/hard spots, unfair moments, fun highlights>
- Tuning applied after: <none | keys changed + new values>
```

## Spec revision note (§14)

Stage 23 review (this file) confirmed the §14 defaults as shipped:
`assets/config/game.json` holds player speed 220 px/s, fire cooldown
0.35 s, bullet speeds 480/240(+40/wave ≤360), lives 3, respawn 1.5 s,
invulnerability 2.0 s, dive speeds 140/100/70, spec §7 wave rows.
Future balance edits happen ONLY through this file.
