import re
import main as m

pid = m.find_roblox_process()
print("pid", pid)
r = m.Reader(pid)
base = r.base_address
img, covered = r.read_image()
print("base 0x%X size %d covered %d" % (base, len(img), covered))

sec = m.parse_sections(img)
rdata = [(v["start"], v["end"]) for k, v in sec.items() if k == ".rdata"]
code = [(v["start"], v["end"]) for k, v in sec.items() if k == ".text"]


def resolve(name_bytes):
    descs = m.find_type_descriptors(img, base, name_bytes)
    if not descs:
        return []
    locs = []
    for d in descs:
        locs.extend(m.find_complete_object_locators(img, base, d, rdata))
    vts = []
    for l in locs:
        vts.extend(m.find_vtables(img, base, l, rdata, code))
    return sorted(set(v - base for v in vts))


keywords = [b"FastCluster", b"GeometryD3D11", b"DeviceD3D11", b"RenderableD3D11", b"DeviceContext"]
seen = {}
for mm in re.finditer(rb"\.\?AV[A-Za-z0-9_@]+@@", img):
    nm = mm.group(0)
    if any(k in nm for k in keywords):
        if nm not in seen:
            seen[nm] = True

print("=== distinct candidate RTTI names ===")
for nm in sorted(seen):
    print("   ", nm.decode("latin-1"))

print("=== vtables ===")
for nm in sorted(seen):
    vts = resolve(nm)
    if vts:
        print("%-60s -> %s" % (nm.decode("latin-1"), ", ".join("0x%X" % v for v in vts)))
