#!/usr/bin/env python3
"""Drives the guest window system through QEMU's QMP (keyboard and mouse injection), reads the app's serial log and
checks screenshots pixel by pixel.   usage: gui_driver.py <qmp.sock> <serial.log> <outdir>
Prints 'GUITEST PASS|FAIL: ...' lines and a final 'GUITEST DONE fail=N'."""
import json, os, re, socket, sys, time
from PIL import Image

sock_path, log_path, outdir = sys.argv[1:4]
os.makedirs(outdir, exist_ok=True)
fails = 0

def result(ok, name, detail=""):
    global fails
    if not ok: fails += 1
    print(f"GUITEST {'PASS' if ok else 'FAIL'}: {name}" + (f" ({detail})" if detail and not ok else ""), flush=True)
    return ok

class QMP:
    def __init__(self, path, timeout=300):
        t0 = time.time()
        while True:
            try: self.s = socket.socket(socket.AF_UNIX); self.s.connect(path); break
            except OSError:
                if time.time() - t0 > timeout: raise
                time.sleep(0.3)
        self.f = self.s.makefile("rw"); self.f.readline(); self.cmd("qmp_capabilities")
    def cmd(self, name, **args):
        self.f.write(json.dumps({"execute": name, "arguments": args}) + "\n"); self.f.flush()
        while True:
            r = json.loads(self.f.readline())
            if "return" in r or "error" in r: return r

def log(): return open(log_path, "rb").read().decode("latin1").replace("\r", "")
def wait_log(pattern, since=0, timeout=120):
    t0 = time.time()
    while time.time() - t0 < timeout:
        m = re.search(pattern, log()[since:])
        if m: return m
        time.sleep(0.3)
    return None

q = QMP(sock_path)
def key(*names, gap=0.25):
    for n in names:
        q.cmd("send-key", keys=[{"type": "qcode", "data": n}], **{"hold-time": 60}); time.sleep(gap)
def rel(dx, dy):
    while dx or dy:
        sx = max(-100, min(100, dx)); sy = max(-100, min(100, dy)); dx -= sx; dy -= sy
        q.cmd("input-send-event", events=[{"type": "rel", "data": {"axis": "x", "value": sx}}, {"type": "rel", "data": {"axis": "y", "value": sy}}])
        time.sleep(0.04)
    time.sleep(0.15)
def goto(x, y): rel(-3000, -3000); rel(x, y)              # the kernel clamps at (0,0), then we move exactly
def button(b, down): q.cmd("input-send-event", events=[{"type": "btn", "data": {"button": b, "down": down}}]); time.sleep(0.25)
def click(x, y, b="left"): goto(x, y); button(b, True); button(b, False)
def shot(name):
    time.sleep(0.8)                                         # let the compositor frame land
    ppm = os.path.join(outdir, name + ".ppm"); q.cmd("screendump", filename=ppm)
    im = Image.open(ppm).convert("RGB"); im.save(os.path.join(outdir, name + ".png")); os.remove(ppm); return im
def near(p, want, tol=4): return all(abs(a - b) <= tol for a, b in zip(p, want))

ORANGE, GRAY, GREEN, BLUE = (0xD0, 0x80, 0x00), (0x55, 0x55, 0x55), (0x2E, 0x7D, 0x32), (0x15, 0x65, 0xC0)

if not wait_log(r"GUIDEMO READY", timeout=420): result(False, "guest GUI app started"); print("GUITEST DONE fail=%d" % fails); sys.exit(1)
result(True, "guest GUI app started (window system up, windows created)")
L = log()
result("GUIDEMO screen 1024x768" in L, "framebuffer 1024x768 reported to the app")
result(bool(re.search(r'\[gui \] window \d+ "Alpha" pid \d+ content \(200,120\) 300x180', L)) and
       bool(re.search(r'\[gui \] window \d+ "Beta" pid \d+ content \(320,200\) 300x180', L)), "windows created at the requested positions")
time.sleep(1.0)

# 1. typing goes to the focused window (Beta was created last)
mark = len(log()); key("h", "i")
m = wait_log(r"win=Beta type=key_down code=105 char=i", mark, 30)
result(bool(m) and "win=Beta type=key_down code=104 char=h" in log()[mark:], "keyboard: 'h','i' delivered to the focused window (Beta)")
im = shot("gui1")
result(near(im.getpixel((210, 130)), GREEN), "Alpha content drawn (green)", str(im.getpixel((210, 130))))
result(near(im.getpixel((600, 370)), BLUE), "Beta content drawn (blue)", str(im.getpixel((600, 370))))
result(near(im.getpixel((450, 182)), ORANGE), "focused window (Beta) has the orange title bar", str(im.getpixel((450, 182))))
result(near(im.getpixel((350, 102)), GRAY), "unfocused window (Alpha) has the gray title bar", str(im.getpixel((350, 102))))
result(near(im.getpixel((450, 270)), BLUE), "Beta is in front where the windows overlap", str(im.getpixel((450, 270))))

# 2. click on the button inside Alpha: raises + focuses it, the app counts the click
mark = len(log()); click(260, 245)
m = wait_log(r"win=Alpha type=mouse_down x=60 y=125 btn=1", mark, 30)
result(bool(m), "mouse: click delivered with window-relative coordinates")
result(bool(wait_log(r"win=Alpha type=focus gained=1", mark, 10)) and bool(wait_log(r"win=Beta type=focus gained=0", mark, 10)), "focus moves to the clicked window")
mark = len(log()); key("x")
result(bool(wait_log(r"win=Alpha type=key_down code=120 char=x", mark, 30)), "keyboard now goes to Alpha")
mark = len(log()); button("wheel-up", True); button("wheel-up", False)
result(bool(wait_log(r"win=Alpha type=wheel delta=1", mark, 20)), "mouse wheel up delivered (delta=+1)")
im = shot("gui2")
result(near(im.getpixel((350, 102)), ORANGE), "Alpha title bar turned orange", str(im.getpixel((350, 102))))
result(near(im.getpixel((560, 182)), GRAY), "Beta title bar turned gray", str(im.getpixel((560, 182))))
result(near(im.getpixel((450, 270)), GREEN), "Alpha raised above Beta in the overlap", str(im.getpixel((450, 270))))
white = sum(1 for xx in range(212, 300) for yy in range(162, 172) if im.getpixel((xx, yy)) == (255, 255, 255))
result(white > 20, "app redrew its window after the click ('clicks: 1' text)", f"white pixels={white}")

# 3. drag Beta by its title bar
goto(560, 189); button("left", True); rel(60, 100); button("left", False)
im = shot("gui3")
result(near(im.getpixel((500, 289)), ORANGE), "dragged window's title bar is at its new position", str(im.getpixel((500, 289))))
old = im.getpixel((560, 182))
result(not near(old, ORANGE) and not near(old, GRAY) and old[2] > old[0], "old position shows the desktop again", str(old))
result(near(im.getpixel((600, 400)), BLUE), "dragged window content moved with it", str(im.getpixel((600, 400))))

# 4. close buttons: the OS sends a close request, the app destroys its window
mark = len(log()); click(489, 109)
result(bool(wait_log(r"win=Alpha type=close", mark, 30)), "close button sends a close event")
im = shot("gui4")
result(not near(im.getpixel((350, 200)), GREEN), "closed window disappeared from the screen", str(im.getpixel((350, 200))))
mark = len(log()); click(669, 289)
result(bool(wait_log(r"win=Beta type=close", mark, 30)) and bool(wait_log(r"GUIDEMO DONE", mark, 30)), "closing the last window ends the app cleanly")
im = shot("gui5")
result(near(im.getpixel((600, 400)), BLUE) is False, "all app windows gone", str(im.getpixel((600, 400))))
print("GUITEST DONE fail=%d" % fails, flush=True)
sys.exit(1 if fails else 0)
