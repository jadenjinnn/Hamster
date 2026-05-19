# Feature spec: Flappy Bird sample project

> Tier: **Sketch**
> Status: **approved**
> Started: 2026-05-19
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated
- **Tier rationale (1 sentence)**: The game lives entirely inside a single Hamster project directory and changes no engine source, so failure or rework is `rm -rf <projectdir>` cheap — Sketch tier matches the risk.

---

## Problem

The engine has shipped roughly twenty features over Phase 5 (physics, animations, sidecars, UI, batching, spatial index) but has never been used end-to-end to produce a real, playable game. Without a complete sample, there is no proof — for the author or for recruiters — that the parts compose. Flappy Bird is the smallest game that exercises gravity, input, runtime entity spawning, collisions, scrolling, score UI, and a game-over restart loop, so it doubles as both a confidence-builder and the bundled sample for the upcoming Phase 6 packaging release.

## In scope

- A new Hamster project `FlappyBird` (location TBD at start of implementation — most likely under `%USERPROFILE%/HamsterProjects/FlappyBird` so the project registry picks it up like any other project).
- One scene containing: bird entity, ground entity, ceiling entity, score UIText, game-over UIButton (initially disabled / hidden), one manager entity that hosts the game-state script.
- Bird: dynamic Box2D body, no horizontal motion, flap = upward impulse on spacebar.
- Pipes: spawned at runtime by the manager script every N seconds; two static (or kinematic) bodies per pipe with a vertical gap; assigned a leftward velocity; destroyed when their x position passes the left edge of the play area.
- Ground + ceiling: static collider strips that end the run on contact.
- Game state machine in one Python manager script: `Idle → Playing → GameOver → Playing → …`.
  - `Idle` (after press Play): bird hovering, gravity effectively off (body type `Static` or `Kinematic`, switched to `Dynamic` on first space), no pipes spawning, score hidden or `0`, game-over button hidden.
  - `Playing`: bird falls, flap impulse on space, pipes spawn + scroll + are scored when their right edge crosses the bird's x, score increments.
  - `GameOver`: on collision, freeze the bird (zero velocity, kinematic), stop pipe spawning, freeze existing pipes, show the game-over button.
  - Restart (button click) → reset bird position, destroy all pipes, reset score to 0, return to `Idle` (or straight to `Playing`, decided at implementation).
- Score UIText anchored Top-Centre.
- Game-over UIButton anchored Centre, only visible in `GameOver` state.
- Solid-colour sprites — bird, pipes, ground, ceiling are tinted `Sprite` components with no texture (engine already supports this). No PNG asset pipeline for v1.
- A short `README.md` inside the project explaining "press space to flap".

## Out of scope

- Audio (no sound effects, no music).
- Animations (no flap wing-tilt, no death animation).
- Parallax background, scrolling clouds, day/night cycle.
- Difficulty ramp (constant pipe gap and speed for v1).
- Persistent high score across runs (no file I/O from the script).
- Pause / resume.
- Quit-to-menu button. Restart only.
- Mobile / gamepad input. Spacebar only.
- PNG textures. Solid-colour tints only.
- Engine source changes. If implementation surfaces a missing engine capability (e.g. "no Python API to clear all dynamic entities at once"), we stop and either spec a small engine feature separately or work around it in the script — we do not silently add to the engine inside this feature.

## API sketch

```python
# manager.py — the single game-state script on the Manager entity.
import Hamster

class Manager(Hamster.HamsterBehaviour):
    IDLE, PLAYING, GAMEOVER = 0, 1, 2

    def on_create(self):
        self.state = self.IDLE
        self.score = 0
        self.pipe_timer = 0.0
        self.bird = self.find_entity_by_name("Bird")
        self.score_text = self.find_entity_by_name("Score")
        self.game_over_btn = self.find_entity_by_name("GameOverButton")
        # hide game-over button in idle state
        # (UIButton visibility handled by mutating colour alpha — simplest path
        #  unless we surface a `visible` flag during implementation)

    def on_update(self, dt):
        if self.state == self.PLAYING:
            self.pipe_timer += dt
            if self.pipe_timer > 1.5:
                self._spawn_pipe()
                self.pipe_timer = 0.0
            # cull off-screen pipes, score pipes the bird just passed

        if self.key_pressed(Hamster.key_code.SPACE):
            if self.state == self.IDLE:
                self._start_playing()
            elif self.state == self.PLAYING:
                self.bird.apply_impulse(0, 8)  # numbers TBD

    def on_collision(self, other_uuid):
        if self.state == self.PLAYING:
            self._game_over()

    def on_button_clicked(self, uuid):
        if uuid == self.game_over_btn.uuid and self.state == self.GAMEOVER:
            self._restart()
```

```python
# bird.py — minimal, only exists to forward collisions to the manager.
# (Alternative: put the on_collision on the Manager and have it find the bird
#  by name. Pick at implementation. The spec is agnostic.)
```

---

## Why this approach

**One project, one manager script, solid-colour sprites.** Alternatives considered:

- *Multiple scripts (BirdController, PipeSpawner, ScoreKeeper, GameOver)* — cleaner separation but introduces cross-script coordination (events / shared state) that the engine has no first-class primitive for. The manager-pattern is what every Phase 5 smoke test uses already, so we know it works.
- *PNG art assets* — looks better but requires a tool to make sprites, a copy step into the project dir, and asset import friction. v1 wants "runs end to end with one Python file" as the proof. Polish is future work.
- *Stop+Play restart (option 2 from design conversation)* — would re-use the simulation-snapshot feature for a free reset, but it needs a "request stop simulation" Python API that may not exist. Default to **option 1 (reset in-place)** because it works against today's engine surface. If a script-side stop API turns out to already exist, we can switch trivially.

Tradeoffs accepted: no audio, no polish, no high-score persistence. The game must feel playable enough that a recruiter clicking through it understands what they're seeing — that's the bar, not "shippable on Steam".

## Risks / what could go wrong

1. **Restart leaves stale entities.** Resetting in-place means the manager must destroy *every* pipe before returning to Idle. Forgetting one means a ghost pipe survives across runs. Mitigation: track spawned pipes in a list, iterate on restart.
2. **`on_button_clicked` fires while in IDLE because the button is still pickable.** Game-over button needs to be either hidden (not just transparent) or guarded by the state machine. Engine has no `UIButton.visible` flag today — workaround is the state-machine guard, but a curious player who can find the button somehow still triggers it.
3. **Score-when-passed is off-by-one.** Comparing pipe x against bird x has a per-frame discretisation; if pipe scroll speed is fast we may skip the crossing frame. Mitigation: store `last_x` per pipe and check the sign of `last_x - bird.x` flipping.
4. **Solid-colour sprite without a texture may not actually render.** The Sprite component has a `colour` field but the renderer path may multiply by texture sample — needs a 30-second check during impl, may need a 1×1 white texture asset baked into the project.
5. **First-spacebar-after-Play is also "flap"** — pressing Space transitions Idle→Playing AND fires the impulse in the same frame. May be desired ("press space to start = first flap"), may not. Cheap to handle either way; flag during impl.
6. **Bird body type switching (Kinematic → Dynamic) at Play start.** Python API for changing `BodyType` at runtime may not exist; today's API mostly only mutates velocity/forces. Fallback: keep bird Dynamic always and use `set_velocity(0,0)` every frame during Idle to fake gravity-off.

## Success criteria

This feature is "shipped" when **all of the following** are observably true:

1. Opening the FlappyBird project in the editor shows the scene with a bird, ground, ceiling, score text, and game-over button (latter hidden / dimmed).
2. Pressing the editor's **Play** button transitions to the Idle state — bird is visible, not falling.
3. Pressing **Space** starts the game; bird begins falling.
4. Subsequent Space presses produce visible upward impulse (bird arcs up and back down).
5. Pipes appear on the right edge of the play area, scroll left across the viewport, and disappear once they leave the left edge.
6. Bird passing through a pipe gap increments the on-screen score by 1.
7. Bird touching a pipe, the ground, or the ceiling transitions to GameOver: bird stops moving, pipe spawning stops, the game-over button becomes visible / interactive.
8. Clicking the game-over button returns the game to a playable Idle state with score reset and the previous pipes gone.
9. The Stop button in the editor returns the scene to its pre-Play state cleanly (simulation-snapshot covers this).
10. The smoke test still passes (27/27 or higher if we extend it — see below).

## Test extensions required

- **Add one smoke scenario** that loads the FlappyBird project's `manager.py` (or a stripped-down version of it) into the smoke-test driver and runs ~60 frames in the IDLE → PLAYING → GAMEOVER → restart path, asserting:
  - Score increments at least once if we artificially position a pipe and step frames.
  - GameOver state is reachable by colliding the bird with a ground entity.
  - Restart resets `score == 0` and bird position to the start.
- If a stripped-down driver script is too coupled to the project layout, downgrade this to a single "import manager.py succeeds and class instantiates" smoke test, on the grounds that the rest is integration-level and verified by manual play. State this fallback under "Decisions during implementation" if it lands.
- Manual verification checklist matches the 10 success criteria above. Author runs through all 10 before marking shipped.

---

## Decisions during implementation

<!-- Append-only. -->

## Spec amendments

<!-- Append-only. -->

### 2026-05-19 — switch from solid-colour placeholders to PNG sprites + 3-frame bird animation

Original spec defaulted to solid-colour `Sprite` tints + no animation to keep the asset pipeline out of v1. Author has sprites and wants the bird animated.

**In scope (added):**
- Author hand-imports PNG sprite assets (bird × 3 frames, top + bottom pipe, ground, ceiling, background) into the project via the editor's Asset Browser.
- Author builds the `bird_flap` 3-frame `.hanim` animation in the AnimationPanel.
- Bird `Animation` component plays `bird_flap` on a loop during Idle and Playing; `stop_animation` called on GameOver.
- Scripts resolve textures by name via `EntityHandle.set_texture("name")` to stay decoupled from import order / UUIDs.
- Scene + entity authoring is done by the author in the editor; Claude does **not** generate a binary scene file (engine's serialise format is raw binary, fragile to hand-author). A README in the project gives the step-by-step setup.

**In scope (removed):**
- "Solid-colour sprites — bird, pipes, ground, ceiling are tinted `Sprite` components with no texture..."

**Out of scope (removed):**
- "PNG textures. Solid-colour tints only."
- "Animations (no flap wing-tilt, no death animation)."

**Out of scope (added):**
- Death animation (rotating bird falling). Future work.
- Bundling the FlappyBird project into the engine repo / installer payload — defers to the packaging spec.

**Risks (removed):**
- Original risk #4 (Sprite-with-colour-no-texture rendering path) — no longer relevant.

**Risks (added):**
- Bird animation looping during GameOver looks wrong on a frozen bird. Manager must call `stop_animation` on transition into GameOver.
- `set_texture("bird_1")` with no matching imported texture is silent — sprite renders blank. Mitigation: script logs a clear error when a named lookup fails; README documents the exact names to use.
- `animate("bird_flap")` will fail if the `.hanim` isn't authored yet. Mitigation: defensive `is_animating` / try-except in the manager so first-run-before-animation-is-built isn't a crash.

Approval: granted 2026-05-19.

### 2026-05-19 — add transform / set_velocity / apply_impulse to EntityHandle

Discovered during pre-implementation API audit: `EntityHandle` only exposes UUID, `add_component`, asset setters, and hierarchy. It has **no transform read/write**, **no velocity API**, and **no impulse API**. Without these, the manager can't:
- Read pipe positions to detect off-screen → can't cull pipes.
- Set pipe initial velocity → pipes can't scroll.
- Attach a per-pipe Behaviour script either — `add_component` only handles `Sprite` and `Rigidbody`.

Workarounds inside the existing API were all dead-ended (recycling pre-placed pipes still needs transform reads; dropping pipes makes FB not-FB).

**Amendment**: add three methods to `Hamster-Py/src/EntityHandle.h`:
- `entity.transform` — property, get/set. Reads/writes the Transform component on the target UUID via the scene's registry. Returns by-value (matching `self.transform` semantics on HamsterBehaviour).
- `entity.set_velocity(vx, vy)` — writes `pendingVelocity` + `hasPendingVelocity` flag on the Rigidbody. Raises `ValueError` if the entity has no Rigidbody.
- `entity.apply_impulse(ix, iy)` — adds to `pendingImpulse` on the Rigidbody. Same guard.

These are additive to a struct that already owns the UUID + scene + app pointers. No save-format change. Useful for any non-trivial game script, not FB-specific.

Cost: ~30 lines of bindings + a rebuild of `Hamster.pyd` and recopy.

Approval: granted 2026-05-19.

---

## Future work (out-of-scope ideas surfaced during this feature)

- Audio system (would need a new engine feature; SFX on flap / score / death).
- Animations on the bird (wing flap, death rotation).
- Parallax background using batched sprites.
- Persistent high-score file (would need a script-accessible filesystem API).
- PNG asset pipeline polish — drag-drop into project, hot-reload.
- Bundled into the upcoming Windows installer (separate **packaging** feature spec).
- Decide whether the FlappyBird project lives in the repo (as `samples/FlappyBird/`) or only in the installer payload — relates to packaging spec.
