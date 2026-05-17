"""
Sprite-batching benchmark.

Spawns NUM_SPRITES entities at runtime, each with a Sprite component
using one of the textures in TEXTURE_NAMES. Watch the draw-call HUD in
the top-right of the Level Editor to read the per-frame draw count.

How to use:
1. Copy this file into your Hamster project's root (next to your other
   scripts).
2. In the editor, create a new entity in the scene. Attach this script
   to its Behaviour component via the Property Editor's Add Script popup.
3. Make sure TEXTURE_NAMES below match textures that exist in your
   project. New projects ship with 'square', 'triangle', and 'circle'.
4. Hit Play. The HUD's "N draws" counter shows the result.

Texture names in fresh projects: "Square", "Triangle", "Circle"
(set in Project.cpp default-texture init).

Measurements (record these for the resume bullet):
  - v1 (same texture):   TEXTURE_NAMES = ["Square"]
                         expected = 1 draw call for NUM_SPRITES sprites
  - v2 (sampler array):  TEXTURE_NAMES = ["Square", "Triangle", "Circle"]
                         expected = 1 draw call (3 textures fit in 16 slots)
  - Baseline:            stash this feature's changes, rebuild, run again.
                         Expected = NUM_SPRITES draw calls.

Reset the scene between runs (Stop → simulation-snapshot reverts the
spawned entities) so the baseline measurement starts from the same state.
"""

import Hamster

NUM_SPRITES = 5000
TEXTURE_NAMES = ["Square"]


class BenchmarkSpawner(Hamster.HamsterBehaviour):
    def on_create(self):
        cols = 100
        for i in range(NUM_SPRITES):
            x = (i % cols) * 12.0
            y = (i // cols) * 12.0
            t = Hamster.Transform(
                Hamster.vec3(x, y, 0),
                0.0,
                Hamster.vec2(8, 8),
            )
            e = self.create_entity(f"bs_{i}", t)
            e.add_component(Hamster.Sprite(Hamster.vec3(1.0, 0.6, 0.3)))
            e.set_texture(TEXTURE_NAMES[i % len(TEXTURE_NAMES)])

    def on_update(self, dt):
        pass
