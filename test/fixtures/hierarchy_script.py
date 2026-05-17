import os
import Hamster


class HierarchyTester(Hamster.HamsterBehaviour):
    def on_create(self):
        t = Hamster.Transform(Hamster.vec3(0, 0, 0), 0.0, Hamster.vec2(10, 10))

        # Create a child entity via the new parent= kwarg.
        child = self.create_entity("child", t, parent=self.uuid)

        marker_dir = os.environ.get("HAMSTER_TEST_MARKER_DIR", "")
        ok = True

        kids = self.children
        if len(kids) != 1 or kids[0] != child.uuid:
            ok = False

        if child.parent != self.uuid:
            ok = False

        # Detach the child via set_parent(nil).
        child.set_parent(Hamster.UUID.nil())
        if not Hamster.UUID.is_nil(child.parent):
            ok = False
        if len(self.children) != 0:
            ok = False

        if ok:
            with open(os.path.join(marker_dir, "hierarchy_ok.ok"), "w") as f:
                f.write("ok")

    def on_update(self, dt):
        pass
