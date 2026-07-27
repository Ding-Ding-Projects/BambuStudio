"""On headless desktop: click SearchField tune button, catch the popover HWND,
wait for it to paint, then PrintWindow + screen-BitBlt it. Enumerate its children.
Usage: bldcap.py <searchpanel_hwnd> <local_x> <local_y> <outname>
"""
import ctypes, ctypes.wintypes as wt, sys, time, os, struct, json

u = ctypes.windll.user32
gd = ctypes.windll.gdi32
panel = int(sys.argv[1]); lx = int(sys.argv[2]); ly = int(sys.argv[3]); outname = sys.argv[4]
here = os.path.dirname(os.path.abspath(__file__))
log = []

created = []
WinEventProc = ctypes.WINFUNCTYPE(None, wt.HANDLE, wt.DWORD, wt.HWND, wt.LONG, wt.LONG, wt.DWORD, wt.DWORD)
def cb(hook, ev, hwnd, objid, childid, tid, t):
    if ev in (0x8000, 0x8002) and hwnd and objid == 0:  # OBJECT_CREATE/SHOW, window-level
        cls = ctypes.create_unicode_buffer(64)
        u.GetClassNameW(hwnd, cls, 64)
        rc = wt.RECT(); u.GetWindowRect(hwnd, ctypes.byref(rc))
        created.append((ev, hwnd, cls.value, rc.right-rc.left, rc.bottom-rc.top))
proc = WinEventProc(cb)
h1 = u.SetWinEventHook(0x8000, 0x8002, 0, proc, 0, 0, 0)

msg = wt.MSG()
def pump(dur):
    t0 = time.time()
    while time.time() - t0 < dur:
        while u.PeekMessageW(ctypes.byref(msg), 0, 0, 0, 1):
            u.TranslateMessage(ctypes.byref(msg)); u.DispatchMessageW(ctypes.byref(msg))
        time.sleep(0.003)

def save(hwnd, suffix, mode):
    rc = wt.RECT(); u.GetWindowRect(hwnd, ctypes.byref(rc))
    w, h = rc.right-rc.left, rc.bottom-rc.top
    if w <= 0 or h <= 0:
        return f"{suffix}: badsize {w}x{h}"
    hdc = u.GetDC(0)
    mdc = gd.CreateCompatibleDC(hdc); bmp = gd.CreateCompatibleBitmap(hdc, w, h)
    gd.SelectObject(mdc, bmp)
    if mode == "pw":
        r = u.PrintWindow(hwnd, mdc, 2)
    else:
        r = gd.BitBlt(mdc, 0, 0, w, h, hdc, rc.left, rc.top, 0x00CC0020)
    buflen = w*h*4
    buf = ctypes.create_string_buffer(buflen)
    bih = ctypes.create_string_buffer(struct.pack("<IiiHHIIiiII",40,w,-h,1,32,0,buflen,2835,2835,0,0),40)
    got = gd.GetDIBits(mdc, bmp, 0, h, buf, bih, 0)
    gd.DeleteObject(bmp); gd.DeleteDC(mdc); u.ReleaseDC(0, hdc)
    if got:
        p = os.path.join(here, f"{outname}-{suffix}.bmp")
        with open(p, "wb") as f:
            f.write(b"BM"+struct.pack("<IHHI",54+buflen,0,0,54))
            f.write(struct.pack("<IiiHHIIiiII",40,w,-h,1,32,0,buflen,2835,2835,0,0))
            f.write(buf.raw)
        # brightness check
        nonblack = sum(1 for i in range(0, min(buflen, 400000), 4) if buf.raw[i] or buf.raw[i+1] or buf.raw[i+2])
        return f"{suffix}: {w}x{h} r={r} nonblack_samples={nonblack}"
    return f"{suffix}: dibfail r={r}"

# click tune button on the search panel
lp = ((ly & 0xFFFF) << 16) | (lx & 0xFFFF)
u.PostMessageW(panel, 0x0200, 0, lp)
u.PostMessageW(panel, 0x0201, 1, lp)
u.PostMessageW(panel, 0x0202, 0, lp)
log.append(f"clicked panel {panel} at {lx},{ly}")

# wait for the popup to be created
popup = None
t0 = time.time()
while time.time() - t0 < 3.0:
    pump(0.05)
    for ev, hwnd, cls, w, h in created:
        if 150 < w < 760 and 120 < h < 700:
            popup = hwnd
            break
    if popup:
        break
log.append(f"created={created}")
log.append(f"popup={popup}")

if popup:
    # force it to show/paint
    u.ShowWindow(popup, 5)  # SW_SHOW
    u.UpdateWindow(popup)
    pump(0.4)
    u.RedrawWindow(popup, None, None, 0x0001 | 0x0004 | 0x0100)  # RDW_INVALIDATE|ERASE|UPDATENOW
    pump(0.4)
    log.append(f"vis={u.IsWindowVisible(popup)}")
    log.append(save(popup, "pw", "pw"))
    log.append(save(popup, "scr", "scr"))
    # children
    kids = []
    def echild(hwnd, lparam):
        cls = ctypes.create_unicode_buffer(64); u.GetClassNameW(hwnd, cls, 64)
        txt = ctypes.create_unicode_buffer(128); u.GetWindowTextW(hwnd, txt, 128)
        rc = wt.RECT(); u.GetWindowRect(hwnd, ctypes.byref(rc))
        kids.append({"h": hwnd, "cls": cls.value, "txt": txt.value, "w": rc.right-rc.left, "hh": rc.bottom-rc.top})
        return True
    EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    u.EnumChildWindows(popup, EnumProc(echild), 0)
    log.append("children=" + json.dumps(kids))
    # scroll and capture below-the-fold sections
    for i in range(1, 6):
        targets = [popup]
        def _ec(h, l):
            targets.append(h); return True
        u.EnumChildWindows(popup, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(_ec), 0)
        for tgt in targets:
            u.PostMessageW(tgt, 0x0115, 3, 0)  # WM_VSCROLL SB_PAGEDOWN
        pump(0.6)
        u.RedrawWindow(popup, None, None, 0x0001 | 0x0004 | 0x0100)
        pump(0.3)
        log.append(save(popup, "pw%d" % i, "pw"))
    with open(os.path.join(here, outname + "-hwnd.txt"), "w") as f:
        f.write(str(popup))

u.UnhookWinEvent(h1)
open(os.path.join(here, outname + "-log.txt"), "w", encoding="utf-8").write("\n".join(str(x) for x in log))
