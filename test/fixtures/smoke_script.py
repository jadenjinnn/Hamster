import Hamster
import os

class SmokeTest(Hamster.HamsterBehaviour):
    def on_create(self):
        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", ".")
        with open(os.path.join(marker_dir, "smoke_create.ok"), "w") as f:
            f.write("1")

    def on_update(self, delta_time):
        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", ".")
        with open(os.path.join(marker_dir, "smoke_update.ok"), "w") as f:
            f.write("1")

    def reset_input(self):
        pass
