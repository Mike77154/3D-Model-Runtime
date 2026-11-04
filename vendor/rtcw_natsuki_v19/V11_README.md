# RTCW Natsuki Monster Codec v0.11 — Explicit Animation Preview

## Scope

New **`rtcw_preview`** CLI converts the *offline* `wolfanim.cfg` + `wolfanim.script` graph from v0.10 into a deterministic, manually driven timeline. It can output successive OBJ + wireframe PPM snapshots for MD3/MDC/MDS, two independent MDS channels (LEGS/TORSO), events, and explicit state changes. Source authored independently under CC0 (no RTCW GPL routines copied).

**This is NOT the game's animation AI.** Conditional expressions are *literal selectors provided by the operator*, not boolean expressions evaluated against live game state. `CHANGE` only occurs when the scenario explicitly asks for it. Sound and `showpart/hidepart` flags are logged as *metadata*, not played or used to cull geometry. Animation override durations are preserved in the graph, but do not determine time-based transitions. Priorities/blending semantics of the game are not simulated.

## New modules

- `rtcw_preview.h` / `rtcw_preview.c`: bounded C89 runtime state, exact routing, explicit state transitions and events, per-channel playback clocks, fixed-point frame sampling, loop-tail semantics, pose reconstruction.
- `rtcw_preview_main.c`: offline scenario interpreter (not an AI script interpreter), timeline reporting, OBJ/PPM output, optional multi-PK3 VFS extraction with provenance.
- `tests/test_codec_v11.py`: synthetic MD3/MDS state, loop, interpolator, VFS and negative regression tests.
- `example_preview_plan.txt`: demo scenario. Adjust the script state and animation names to your model/CFG.

## Build

```sh
make clean && make CC=gcc
python3 tests/test_codec_v11.py ./rtcw_preview
```

On Windows, run `BUILD_WINDOWS.bat` in your MinGW32 shell/console with GCC on PATH; native Windows build still needs external verification.

## Local use

```bat
rtcw_preview.exe run body.mds wolfanim.cfg wolfanim.script example_preview_plan.txt output_v11
```

`output_v11` must **not already exist**. It writes `timeline.tsv`, `actions.tsv`, `graph.tsv`, and `pose_NNN.obj` + `pose_NNN.ppm` files. maximum is **96 snapshots**; no viewer window is opened. PPM is a standard binary image format, OBJ is compatible with common mesh viewers.

## Multi-PK3 use

```bat
rtcw_preview.exe run-vfs mounts.txt models/players/example/body.mds example_preview_plan.txt output_modded
```

PK3 entries are resolved according to the priority rules of v0.8. The model's `wolfanim.cfg` (or `animation.cfg`) and `wolfanim.script` are required in the **same virtual directory**. Co-directory pairing is a candidate, not proof of original authorial association. `sources.tsv` records the package used for each file.

## Scenario language (one instruction per line)

```text
# User-directed demonstration; not RTCW AI execution.
STATE COMBAT
PLAY animations WALK default
SHOT
ADVANCE 50
ADVANCE 150
CHANGE ALERT default
SHOT
EVENT DEATH default
ADVANCE 50
```

Syntax:

- `STATE NAME` — explicitly assign preview state (does not run transitions).
- `PLAY animations MOVEMENT CONDITION` — match section `animations`, current state, movement and the *exact* condition string from the graph.
- `PLAY canned_animations MOVEMENT CONDITION` — analogous match in `canned_animations`.
- `EVENT EVENT_NAME CONDITION` — select an event's command by exact match, does **not** modify state by itself.
- `CHANGE NEWSTATE CONDITION` — match a `statechanges` command from current to next state, apply it and change state explicitly.
- `APPLY N` — select one script command by zero-based index; can bypass semantic routing to investigate ambiguity.
- `SHOT` — snapshot at current time.
- `ADVANCE MILLISECONDS` — advance playback and emit a snapshot.

`CONDITION` is the literal remainder of the line, e.g. `WEAPONS PISTOL, ENEMY_POSITION INFRONT`. Its tokens are not parsed into a decision tree. If a selection matches no command or multiple commands, the program returns an error; `APPLY` resolves the latter case manually.

## Playback math and limitations

- Source is strict ISO C89 with static buffers, no `malloc`, `realloc`, `free`, heap allocation, floating point, or `long long` in new runtime modules. Q12 fractional interpolation with milli-FPS and 32-bit `unsigned long`-safe decomposition; maximum 3,600,000 ms (1 hour). 
- `wolfanim.cfg` clips with `loop=0` hold the final frame; `loop>0` loop the *last N frames*. Loops don't change state and explicit script `duration N` doesn't trigger a transition.
- MDS supports independently sampled torso and legs and uses existing v0.4 bone-space interpolation. No faithful implementation of RTCW's blend policies or animation priority arbitration is claimed.
- MD3/MDC supports common pose previews via vertex-space interpolation. **Two unequal torso/legs poses are refused**, because MD3/MDC do not support this skeletal split in the current codec.
- `showpart/hidepart` update an 8-bit **reported** mask only; real geometry visibility is unchanged. Sound references are reported, not emitted.
- Runtime cap: 2048 script commands (v0.10), 512 clips (v0.9), 96 snapshots, 511 input bytes per scenario line, 3,600,000 ms duration.
- Missing/ambiguous/out-of-range clips, malformed scenario instructions and time overflow produce errors instead of fallback animations.
- Failed sequences can leave a partial output directory. Run only on trusted work directories, and do not execute with elevated privileges.
- No commercial RTCW monster assets included or validated. Original user-provided RTCW-SP/RealRTCW/Wwasm ZIPs were used only as technical references.

## Testing

v0.11: 74 named checks, including MDS separate clocks and interpolation, clip loop, state transition, event selection, missing condition, ambiguity, duplicate output destination, malformed scenario bytes, Windows-portable integer-safe math cases, MD3 route restrictions and VFS overrides. Previous suites v0.1 through v0.10 were run without regressions. All 12 executables compiled on Linux with GCC and Clang with `-std=c89 -Wall -Wextra -Wpedantic -Werror`. UBSan on the v0.11 executable passed all 74 checks (Clang). Native Win32, real monster assets and exact original gameplay behavior remain unverified.

## Next phase

v0.12 could add mapping between animation events and visible multi-part attachment rigs, multi-frame contact sheets, and a verified small real game corpus. Avoid automatic interpretation of `wolfanim.script` conditions until the condition grammar and evaluator are separately tested against actual in-game behavior.
