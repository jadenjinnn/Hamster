"""One-off scene migrator: old UIButton/UIText blocks -> new format.

The scene format gained two fields that the deserialiser now *requires*:
  UIButton: + uint8 bold + 16-byte imageUUID   (17 bytes)
  UIText:   + uint8 bold                        (1 byte)

Scenes saved by the older editor lack these trailing bytes, so the new reader
overruns each UI block and corrupts the rest of the parse. This walks the old
stream (old layouts) and rewrites it with the new defaults inserted (bold=0,
imageUUID=nil = 16 zero bytes). Everything else is copied verbatim.

Safety: backs up to <file>.bak, and refuses to write unless the walk consumes
exactly the whole file and every entity ends on the -1 marker.
"""
import struct
import sys
import shutil

# Component IDs (Components.h::ComponentID). ID_ID(5) is never serialised.
TRANSFORM, SPRITE, NAME, RIGIDBODY = 1, 2, 3, 4
BEHAVIOUR, COLLIDER, ANIMATION, HIERARCHY = 6, 7, 8, 9
UIBUTTON, UITEXT = 10, 11
END = -1


class Reader:
    def __init__(self, data):
        self.d = data
        self.i = 0

    def take(self, n):
        b = self.d[self.i:self.i + n]
        if len(b) != n:
            raise EOFError(f"want {n} bytes at {self.i}, have {len(b)}")
        self.i += n
        return b

    def i32(self):
        return struct.unpack_from("<i", self.take(4))[0]

    def u32(self):
        return struct.unpack_from("<I", self.take(4))[0]

    def sizet(self):
        return struct.unpack_from("<Q", self.take(8))[0]


def migrate(path):
    raw = open(path, "rb").read()
    r = Reader(raw)
    out = bytearray()

    def copy(n):
        out.extend(r.take(n))

    copy(16)            # scene UUID
    count = r.u32()     # entity count
    out.extend(struct.pack("<I", count))

    n_btn = n_txt = 0
    for _ in range(count):
        copy(16)        # entity UUID
        while True:
            cid = r.i32()
            out.extend(struct.pack("<i", cid))
            if cid == END:
                break
            elif cid == TRANSFORM:
                copy(24)                      # vec3 + float + vec2
            elif cid == SPRITE:
                copy(28)                      # UUID + vec3
            elif cid == NAME:
                n = r.sizet(); out.extend(struct.pack("<Q", n)); copy(n)
            elif cid == RIGIDBODY:
                copy(24)                      # 2 int + 4 float
            elif cid == COLLIDER:
                copy(16)                      # 2 vec2
            elif cid == HIERARCHY:
                copy(20)                      # UUID + uint32
            elif cid == BEHAVIOUR:
                sc = r.u32(); out.extend(struct.pack("<I", sc))
                for _ in range(sc):
                    copy(16)                  # script UUID
                    n = r.sizet(); out.extend(struct.pack("<Q", n)); copy(n)
            elif cid == ANIMATION:
                ac = r.u32(); out.extend(struct.pack("<I", ac))
                for _ in range(ac):
                    n = r.sizet(); out.extend(struct.pack("<Q", n)); copy(n)
                    copy(16)                  # anim UUID
                dl = r.sizet(); out.extend(struct.pack("<Q", dl)); copy(dl)
                copy(1)                       # loop byte
            elif cid == UIBUTTON:
                copy(1)                       # anchor
                copy(8 + 8 + 1 + 4)           # offset, size, autoSize, padding
                copy(16)                      # bgColour
                n = r.sizet(); out.extend(struct.pack("<Q", n)); copy(n)  # label
                copy(16)                      # textColour
                copy(4 + 1)                   # fontSize, align
                out.extend(b"\x00" * 17)      # NEW: bold + nil imageUUID
                n_btn += 1
            elif cid == UITEXT:
                copy(1)                       # anchor
                copy(8)                       # offset
                n = r.sizet(); out.extend(struct.pack("<Q", n)); copy(n)  # text
                copy(16)                      # textColour
                copy(4 + 4)                   # fontSize, wrapWidth
                out.extend(b"\x00")           # NEW: bold
                n_txt += 1
            else:
                raise ValueError(f"unknown component id {cid} at offset {r.i-4}")

    if r.i != len(raw):
        raise ValueError(f"walk ended at {r.i}, file is {len(raw)} bytes "
                         f"(did not consume everything -- aborting)")

    shutil.copyfile(path, path + ".bak")
    open(path, "wb").write(bytes(out))
    print(f"OK: migrated {n_btn} UIButton, {n_txt} UIText. "
          f"backup -> {path}.bak ({len(raw)} -> {len(out)} bytes)")


if __name__ == "__main__":
    migrate(sys.argv[1])
