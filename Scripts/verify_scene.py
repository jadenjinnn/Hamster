"""Read-only scene parser using the CURRENT (new) format. Prints each entity's
components and stops at the first inconsistency, reporting the byte offset.
Diagnostic only — never writes."""
import struct, sys

T, S, N, RB = 1, 2, 3, 4
BEH, COL, ANIM, HIER, UIB, UIT = 6, 7, 8, 9, 10, 11
NAMES = {1:"Transform",2:"Sprite",3:"Name",4:"Rigidbody",6:"Behaviour",
         7:"Collider",8:"Animation",9:"Hierarchy",10:"UIButton",11:"UIText",-1:"END"}

class R:
    def __init__(s, d): s.d, s.i = d, 0
    def take(s, n):
        b = s.d[s.i:s.i+n]
        if len(b) != n: raise EOFError(f"want {n} at {s.i}, have {len(b)}")
        s.i += n; return b
    def i32(s): return struct.unpack_from("<i", s.take(4))[0]
    def u32(s): return struct.unpack_from("<I", s.take(4))[0]
    def q(s):   return struct.unpack_from("<Q", s.take(8))[0]

def main(path):
    raw = open(path, "rb").read()
    r = R(raw)
    r.take(16); count = r.u32()
    print(f"file={len(raw)}B  scene-uuid+count ok  entities={count}")
    for ei in range(count):
        r.take(16)
        comps = []
        while True:
            off = r.i; cid = r.i32()
            comps.append((cid, off))
            if cid == -1: break
            elif cid == T: r.take(24)
            elif cid == S: r.take(28)
            elif cid == N: r.take(r.q())
            elif cid == RB: r.take(24)
            elif cid == COL: r.take(16)
            elif cid == HIER: r.take(20)
            elif cid == BEH:
                sc = r.u32()
                for _ in range(sc): r.take(16); r.take(r.q())
            elif cid == ANIM:
                ac = r.u32()
                for _ in range(ac): r.take(r.q()); r.take(16)
                r.take(r.q()); r.take(1)
            elif cid == UIB:
                r.take(1+8+8+1+4+16); r.take(r.q()); r.take(16+4+1)  # ...align
                r.take(1); r.take(16)   # NEW: bold + imageUUID
            elif cid == UIT:
                r.take(1+8); r.take(r.q()); r.take(16+4+4)
                r.take(1)               # NEW: bold
            else:
                raise ValueError(f"entity {ei}: unknown component id {cid} at offset {off}")
        print(f"  entity {ei}: " + ", ".join(NAMES.get(c,str(c)) for c,_ in comps))
    print(f"consumed {r.i}/{len(raw)} bytes -> "
          + ("CLEAN (new format)" if r.i == len(raw) else "LEFTOVER BYTES (mismatch)"))

if __name__ == "__main__":
    main(sys.argv[1])
