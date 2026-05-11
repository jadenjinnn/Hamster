import Hamster
import os

class ForceTest(Hamster.HamsterBehaviour):
    def on_create(self):
        pass

    def on_update(self, delta_time):
        self.apply_force(500.0, 0.0)
        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", ".")
        with open(os.path.join(marker_dir, "force_update.ok"), "w") as f:
            f.write("1")

    def reset_input(self):
        pass
