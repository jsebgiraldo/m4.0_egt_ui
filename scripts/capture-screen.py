#!/usr/bin/env python3
"""Capture DRM framebuffer from SAMA5D27-WLSOM1-EK and save as PPM."""
import ctypes, os, struct, sys, mmap, fcntl

OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/screen.ppm"

# ── ioctl helper ─────────────────────────────────────────────────────────────
def _IOC(d, t, n, s):
    return (d << 30) | (s << 16) | (t << 8) | n

DRM_IOCTL_MODE_MAP_DUMB = _IOC(3, 0x64, 0xB3, 16)  # struct drm_mode_map_dumb

# ── libdrm ctypes structs ───────────────────────────────────────────────────
class DrmModeRes(ctypes.Structure):
    _fields_ = [
        ("count_fbs", ctypes.c_int), ("fbs", ctypes.POINTER(ctypes.c_uint32)),
        ("count_crtcs", ctypes.c_int), ("crtcs", ctypes.POINTER(ctypes.c_uint32)),
        ("count_connectors", ctypes.c_int), ("connectors", ctypes.POINTER(ctypes.c_uint32)),
        ("count_encoders", ctypes.c_int), ("encoders", ctypes.POINTER(ctypes.c_uint32)),
        ("min_w", ctypes.c_uint32), ("max_w", ctypes.c_uint32),
        ("min_h", ctypes.c_uint32), ("max_h", ctypes.c_uint32),
    ]

class DrmModeCrtc(ctypes.Structure):
    _fields_ = [
        ("crtc_id", ctypes.c_uint32), ("buffer_id", ctypes.c_uint32),
        ("x", ctypes.c_uint32), ("y", ctypes.c_uint32),
        ("width", ctypes.c_uint32), ("height", ctypes.c_uint32),
        ("mode_valid", ctypes.c_int), ("mode", ctypes.c_byte * 68),
        ("gamma_size", ctypes.c_int),
    ]

class DrmModeFB(ctypes.Structure):
    _fields_ = [
        ("fb_id", ctypes.c_uint32), ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32), ("pitch", ctypes.c_uint32),
        ("bpp", ctypes.c_uint32), ("depth", ctypes.c_uint32),
        ("handle", ctypes.c_uint32),
    ]

# ── Load libdrm ──────────────────────────────────────────────────────────────
drm = ctypes.CDLL("libdrm.so.2")
drm.drmModeGetResources.restype = ctypes.POINTER(DrmModeRes)
drm.drmModeGetCrtc.restype = ctypes.POINTER(DrmModeCrtc)
drm.drmModeGetFB.restype = ctypes.POINTER(DrmModeFB)

fd = os.open("/dev/dri/card0", os.O_RDWR)

# ── Find active CRTC & framebuffer ───────────────────────────────────────────
res = drm.drmModeGetResources(fd)
if not res:
    sys.exit("ERR: drmModeGetResources failed")

crtc = None
for i in range(res.contents.count_crtcs):
    c = drm.drmModeGetCrtc(fd, res.contents.crtcs[i])
    if c and c.contents.buffer_id:
        crtc = c
        break
if not crtc:
    sys.exit("ERR: no active CRTC")

fb = drm.drmModeGetFB(fd, crtc.contents.buffer_id)
if not fb or not fb.contents.handle:
    sys.exit("ERR: cannot get FB handle (DRM master?)")

w = fb.contents.width
h = fb.contents.height
pitch = fb.contents.pitch
bpp = fb.contents.bpp
handle = fb.contents.handle

# ── Map dumb buffer ──────────────────────────────────────────────────────────
# struct drm_mode_map_dumb { u32 handle; u32 pad; u64 offset; }
buf = ctypes.create_string_buffer(16)
struct.pack_into("II", buf, 0, handle, 0)  # handle + pad
fcntl.ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, buf)
offset = struct.unpack_from("Q", buf, 8)[0]

size = pitch * h
mm = mmap.mmap(fd, size, mmap.MAP_SHARED, mmap.PROT_READ, offset=offset)
raw = mm[:]
mm.close()
os.close(fd)

# ── Convert to PPM (RGB888) ─────────────────────────────────────────────────
rgb = bytearray(w * h * 3)
bpp_bytes = bpp // 8

for y in range(h):
    row_off = y * pitch
    for x in range(w):
        src = row_off + x * bpp_bytes
        dst = (y * w + x) * 3
        if bpp == 16:  # RGB565
            val = raw[src] | (raw[src + 1] << 8)
            rgb[dst]     = ((val >> 11) & 0x1F) << 3  # R
            rgb[dst + 1] = ((val >> 5) & 0x3F) << 2   # G
            rgb[dst + 2] = (val & 0x1F) << 3           # B
        elif bpp == 32:  # XRGB8888
            rgb[dst + 2] = raw[src]      # B
            rgb[dst + 1] = raw[src + 1]  # G
            rgb[dst]     = raw[src + 2]  # R

with open(OUT, "wb") as f:
    f.write(f"P6\n{w} {h}\n255\n".encode())
    f.write(bytes(rgb))

print(f"OK {w}x{h} bpp={bpp} -> {OUT}")
