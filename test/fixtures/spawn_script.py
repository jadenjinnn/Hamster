import os
import Hamster

class SpawnManager(Hamster.HamsterBehaviour):
    def on_create(self):
        t = Hamster.Transform(Hamster.vec3(100, 100, 0), 0.0, Hamster.vec2(30, 30))
        entity = self.create_entity("spawned", t)
        entity.add_component(Hamster.Sprite(Hamster.vec3(1, 0, 0)))
        entity.add_component(Hamster.Rigidbody(body_type=Hamster.BodyType.Dynamic, gravity_scale=0.0))
        self.spawned_uuid = entity.uuid

        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", "")
        with open(os.path.join(marker_dir, "spawn_created.ok"), "w") as f:
            f.write("ok")

    def on_update(self, dt):
        self.destroy_entity(self.spawned_uuid)

        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", "")
        with open(os.path.join(marker_dir, "spawn_destroyed.ok"), "w") as f:
            f.write("ok")
