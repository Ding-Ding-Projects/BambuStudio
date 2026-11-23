"""menupopup.py: open a title bar menu by name and capture it, on the headless desktop.
Needs the app launched with BAMBU_LAYOUT_PROBE=1 (driver.py passes it through); the
layout probe answers "menu-popup <Title>" sent as WM_COPYDATA (dwData 2). Run it ON the
desktop:  menupopup.py <frame-hwnd> "<Menu title>" <out-prefix> [extra-wait-seconds]
Writes <out-prefix>_0.bmp and <out-prefix>.txt, then sends Escape to close the popup
(a popup that stays open blocks the next request; ask again and it closes)."""
import ctypes, ctypes.wintypes as W, struct, sys, time
u = ctypes.windll.user32
g = ctypes.windll.gdi32
# Per-monitor DPI aware, or Windows hands this process scaled-down rectangles and
# PrintWindow returns a cropped bitmap of a high-DPI popup.
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
frame, title, out = int(sys.argv[1]), sys.argv[2], sys.argv[3]
log = open(out + ".txt", "w")


def tops():
    hs = []
    cb = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)(lambda h, _: hs.append(h) or True)
    u.EnumWindows(cb, 0)
    return {h for h in hs if u.IsWindowVisible(h)}


class COPYDATASTRUCT(ctypes.Structure):
    _fields_ = [("dwData", ctypes.c_size_t), ("cbData", W.DWORD), ("lpData", ctypes.c_void_p)]


before = tops()
payload = ctypes.create_unicode_buffer("menu-popup " + title)
cds = COPYDATASTRUCT(2, ctypes.sizeof(payload), ctypes.cast(payload, ctypes.c_void_p))
u.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, ctypes.c_void_p]
res = u.SendMessageW(frame, 0x004A, 0, ctypes.addressof(cds))
log.write("sent %s -> %s\n" % (title, res))

found = []
deadline = time.time() + 10
while time.time() < deadline and not found:
    time.sleep(0.25)
    for h in tops() - before:
        r = W.RECT(); u.GetWindowRect(h, ctypes.byref(r))
        if r.right - r.left > 100 and r.bottom - r.top > 100:
            buf = ctypes.create_unicode_buffer(64); u.GetClassNameW(h, buf, 64)
            found.append((h, buf.value, r.right - r.left, r.bottom - r.top))
log.write("popups %s\n" % found)
# The popup grows in with an entry motion: wait until its size has held still for
# a second, then capture at its final size.
settled = []
for h, cls, _, _ in found:
    last, still_since, end = None, time.time(), time.time() + 8
    while time.time() < end:
        r = W.RECT(); u.GetWindowRect(h, ctypes.byref(r))
        size = (r.right - r.left, r.bottom - r.top)
        if size != last:
            last, still_since = size, time.time()
        elif time.time() - still_since > 1.0:
            break
        time.sleep(0.1)
    settled.append((h, cls, last[0], last[1]))
log.write("settled %s\n" % settled)
time.sleep(float(sys.argv[4]) if len(sys.argv) > 4 else 0.3)
for i, (h, cls, wdt, hgt) in enumerate(settled):
    hdc = u.GetWindowDC(h); mdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, wdt, hgt); g.SelectObject(mdc, bmp)
    u.PrintWindow(h, mdc, 2)
    head = struct.pack("<IiiHHIIiiII", 40, wdt, -hgt, 1, 32, 0, wdt * hgt * 4, 0, 0, 0, 0)
    data = ctypes.create_string_buffer(wdt * hgt * 4)
    g.GetDIBits(mdc, bmp, 0, hgt, data, ctypes.create_string_buffer(head, 44), 0)
    with open("%s_%d.bmp" % (out, i), "wb") as f:
        f.write(b"BM" + struct.pack("<IHHI", 54 + len(data), 0, 0, 54) + head + data.raw)
    log.write("saved %s_%d.bmp %s %dx%d\n" % (out, i, cls, wdt, hgt))
    # Close the popup: Escape to every child (the search field holds the focus).
    kids = []
    cb = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)(lambda c, _: kids.append(c) or True)
    u.EnumChildWindows(h, cb, 0)
    for hh in [h] + kids:
        u.PostMessageW(hh, 0x0100, 0x1B, 0x00010001)
        u.PostMessageW(hh, 0x0101, 0x1B, 0xC0010001)
    time.sleep(1.0)
    log.write("closed %s\n" % (not u.IsWindowVisible(h)))
log.write("done\n")
log.close()
