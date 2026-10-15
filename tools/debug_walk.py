import struct
import main as m

pid = m.find_roblox_process()
r = m.Reader(pid)
base = r.base_address
print("pid", pid, "base 0x%X" % base)


def u64(a):
    b = r.try_read(a, 8)
    return struct.unpack("<Q", b)[0] if b and len(b) >= 8 else 0


def u32(a):
    b = r.try_read(a, 4)
    return struct.unpack("<I", b)[0] if b and len(b) >= 4 else 0


def cstr(a, n=64):
    b = r.try_read(a, n)
    if not b:
        return ""
    z = b.find(b"\x00")
    return b[:z if z >= 0 else len(b)].decode("latin-1", "replace")


def class_name(inst):
    cd = u64(inst + 0x18)
    if not cd:
        return ""
    sp = u64(cd + 0x8)
    if not sp:
        return ""
    ln = u32(sp + 0x10)
    if ln == 0 or ln > 128:
        return ""
    if ln <= 15:
        return cstr(sp, ln)
    s = u64(sp)
    return cstr(s, ln) if s else ""


def children(p):
    vec = u64(p + 0x78)
    if not vec:
        return []
    begin = u64(vec)
    end = u64(vec + 8)
    if not begin or end <= begin:
        return []
    n = (end - begin) // 16
    if n > 10000:
        n = 10000
    buf = r.try_read(begin, n * 16)
    out = []
    if buf:
        for i in range(min(n, len(buf) // 16)):
            c = struct.unpack_from("<Q", buf, i * 16)[0]
            if c:
                out.append(c)
    return out


fakeDM = u64(base + 0x8dc8000)
print("fakeDM 0x%X" % fakeDM)
dm = u64(fakeDM + 0x1f8) if fakeDM else 0
print("DataModel 0x%X" % dm)

if dm:
    kids = children(dm)
    print("dm children:", len(kids))
    svc = {}
    for c in kids:
        svc.setdefault(class_name(c), c)
    print("services:", sorted(svc.keys()))
    players = svc.get("Players", 0)
    print("Players 0x%X" % players)
    if players:
        lp = u64(players + 0x120)
        print("LocalPlayer 0x%X class=%s" % (lp, class_name(lp)))
        pcs = children(players)
        print("player children:", len(pcs))
        for c in pcs[:20]:
            model = u64(c + 0x298)
            print("  player 0x%X class=%s model 0x%X" % (c, class_name(c), model))
            if model:
                mcs = children(model)
                print("     model class=%s children=%d %s" %
                      (class_name(model), len(mcs), [class_name(x) for x in mcs][:12]))
