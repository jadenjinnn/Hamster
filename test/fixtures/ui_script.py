import os
import Hamster

# Smoke fixture for game-ui Phase B.
#
# Scenario: a manager script that
#   1. on_create: looks up two entities by name ("TestBtn" / "Counter"),
#      writes a marker on success, and mutates the counter's text once.
#   2. on_button_clicked: writes a marker recording the clicked UUID.
#
# The smoke driver synthesises the click by posting ButtonClickedEvent
# directly via the dispatcher — same as UI-4.
class UISmokeManager(Hamster.HamsterBehaviour):
    def on_create(self):
        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", "")

        self.btn = self.find_entity_by_name("TestBtn")
        self.counter = self.find_entity_by_name("Counter")

        if self.btn is None or self.counter is None:
            return

        # Mutate the counter's text — exercises EntityHandle.set_text.
        self.counter.set_text("Score: 1")

        with open(os.path.join(marker_dir, "ui_create.ok"), "w") as f:
            f.write(str(self.btn.uuid))

    def on_update(self, dt):
        pass

    def on_button_clicked(self, uuid):
        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", "")
        if self.btn is None:
            return
        if uuid == self.btn.uuid:
            with open(os.path.join(marker_dir, "ui_clicked.ok"), "w") as f:
                f.write(str(uuid))
