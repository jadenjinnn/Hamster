import Hamster


class SnapshotProbe(Hamster.HamsterBehaviour):
    def on_create(self):
        t = Hamster.Transform(Hamster.vec3(50, 50, 0), 0.0, Hamster.vec2(10, 10))
        for i in range(5):
            self.create_entity(f"runtime_{i}", t)

    def on_update(self, dt):
        self.transform.position = Hamster.vec3(999, 999, 0)
