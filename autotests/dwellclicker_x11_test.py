# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Resting the pointer clicks with dwell clicking enabled, with compositing on and off."""

import ctypes as C
import ctypes.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

BUTTON_PRESS = 4
BUTTON_RELEASE = 5
MAP_NOTIFY = 19
BUTTON_PRESS_MASK = 1 << 2
BUTTON_RELEASE_MASK = 1 << 3
STRUCTURE_NOTIFY_MASK = 1 << 17


class XButtonEvent(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong), ("send_event", C.c_int), ("display", C.c_void_p),
                ("window", C.c_ulong), ("root", C.c_ulong), ("subwindow", C.c_ulong), ("time", C.c_ulong),
                ("x", C.c_int), ("y", C.c_int), ("x_root", C.c_int), ("y_root", C.c_int),
                ("state", C.c_uint), ("button", C.c_uint), ("same_screen", C.c_int)]


class XEvent(C.Union):
    _fields_ = [("type", C.c_int), ("xbutton", XButtonEvent), ("pad", C.c_long * 24)]


def load_x11():
    x11 = C.CDLL(ctypes.util.find_library("X11"))
    xtst = C.CDLL(ctypes.util.find_library("Xtst"))
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XDefaultRootWindow.argtypes = [C.c_void_p]
    x11.XDefaultRootWindow.restype = C.c_ulong
    x11.XCreateSimpleWindow.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_uint,
                                        C.c_ulong, C.c_ulong]
    x11.XCreateSimpleWindow.restype = C.c_ulong
    x11.XSelectInput.argtypes = [C.c_void_p, C.c_ulong, C.c_long]
    x11.XMapWindow.argtypes = [C.c_void_p, C.c_ulong]
    x11.XPending.argtypes = [C.c_void_p]
    x11.XNextEvent.argtypes = [C.c_void_p, C.POINTER(XEvent)]
    x11.XFlush.argtypes = [C.c_void_p]
    x11.XSync.argtypes = [C.c_void_p, C.c_int]
    x11.XCloseDisplay.argtypes = [C.c_void_p]
    x11.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong, C.c_int, C.c_int,
                                          C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]
    xtst.XTestFakeMotionEvent.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_ulong]
    x11.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_ulong, C.c_int]
    x11.XGetImage.restype = C.POINTER(XImage)
    return x11, xtst


DWELL_TIME = 600


class XImage(C.Structure):
    _fields_ = [("width", C.c_int), ("height", C.c_int), ("xoffset", C.c_int), ("format", C.c_int),
                ("data", C.POINTER(C.c_ubyte)), ("byte_order", C.c_int), ("bitmap_unit", C.c_int),
                ("bitmap_bit_order", C.c_int), ("bitmap_pad", C.c_int), ("depth", C.c_int),
                ("bytes_per_line", C.c_int), ("bits_per_pixel", C.c_int)]


def ring_pixels(x11, display, root, cx, cy, radius=30):
    """Counts pixels around (cx, cy) in the ring's default red over the white test window."""
    image = x11.XGetImage(display, root, cx - radius, cy - radius, 2 * radius, 2 * radius, 0xffffffff, 2)
    assert image, "XGetImage failed"
    img = image.contents
    assert img.bits_per_pixel == 32, img.bits_per_pixel
    count = 0
    for row in range(img.height):
        for col in range(img.width):
            offset = row * img.bytes_per_line + col * 4
            blue, green, red = img.data[offset], img.data[offset + 1], img.data[offset + 2]
            count += red > 200 and green < 170 and blue < 170
    return count


def dbus_call(gdbus, path, method, *arguments):
    result = subprocess.run([gdbus, "call", "--session", "--dest", "org.kde.KWin", "--object-path", path,
                             "--method", method, *arguments], capture_output=True, text=True, timeout=5)
    return result.stdout.strip() if result.returncode == 0 else result.stderr.strip()


def run(wm_path, gdbus, compositing, x11, xtst):
    with tempfile.TemporaryDirectory(prefix="sonic-dwell-") as temp:
        home = Path(temp)
        runtime = home / "runtime"
        runtime.mkdir(mode=0o700)
        for name in ("config", "cache", "data", "state"):
            (home / name).mkdir()
        (home / "config" / "kwinrc").write_text(
            f"[Compositing]\nEnabled={'true' if compositing else 'false'}\n"
            "[Plugins]\ndwellclickerEnabled=true\n"
            f"[Effect-dwellclicker]\nDelayTime=50\nDwellTime={DWELL_TIME}\nMotionThreshold=5\n")
        environment = os.environ.copy()
        environment.update(QT_QPA_PLATFORM="xcb", QT_QPA_PLATFORMTHEME="generic", QT_STYLE_OVERRIDE="Fusion",
                           QT_PLUGIN_PATH=os.pathsep.join(filter(None, [str(Path(wm_path).parent),
                                                                        os.environ.get("QT_PLUGIN_PATH")])),
                           HOME=temp, XDG_CONFIG_HOME=str(home / "config"), XDG_CACHE_HOME=str(home / "cache"),
                           XDG_DATA_HOME=str(home / "data"), XDG_STATE_HOME=str(home / "state"),
                           XDG_RUNTIME_DIR=str(runtime), LIBGL_ALWAYS_SOFTWARE="1", KDE_DEBUG="1")

        display = x11.XOpenDisplay(None)
        assert display, "Xvfb must be running"
        root = x11.XDefaultRootWindow(display)
        wm = None
        with tempfile.TemporaryFile(mode="w+") as log:
            try:
                wm = subprocess.Popen([wm_path], env=environment, stdout=log, stderr=log)
                deadline = time.monotonic() + 20
                while not (runtime / "dwellclicker.sock").exists():
                    assert wm.poll() is None, f"WM exited with {wm.returncode}"
                    assert time.monotonic() < deadline, "Timed out waiting for the dwell clicker socket"
                    time.sleep(0.05)

                window = x11.XCreateSimpleWindow(display, root, 100, 100, 400, 300, 0, 0, 0xffffff)
                x11.XSelectInput(display, window, BUTTON_PRESS_MASK | BUTTON_RELEASE_MASK | STRUCTURE_NOTIFY_MASK)
                x11.XMapWindow(display, window)
                x11.XFlush(display)

                event = XEvent()
                seen = []
                mapped = False
                deadline = time.monotonic() + 20
                next_move = 0.0
                offset = 0
                while (BUTTON_PRESS, 1) not in seen or (BUTTON_RELEASE, 1) not in seen:
                    assert wm.poll() is None, f"WM exited with {wm.returncode}"
                    assert time.monotonic() < deadline, f"no dwell click arrived; events seen: {seen}"
                    while x11.XPending(display):
                        x11.XNextEvent(display, C.byref(event))
                        if event.type == MAP_NOTIFY:
                            mapped = True
                        elif event.type in (BUTTON_PRESS, BUTTON_RELEASE):
                            seen.append((event.type, event.xbutton.button))
                    if mapped and time.monotonic() >= next_move:
                        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
                        x11.XTranslateCoordinates(display, window, root, 200, 150, C.byref(x), C.byref(y), C.byref(child))
                        # Rest, then move past the threshold so the dwell starts.
                        xtst.XTestFakeMotionEvent(display, -1, x.value + offset, y.value, 0)
                        x11.XSync(display, 0)
                        time.sleep(0.1)
                        offset = 20 if offset == 0 else 0
                        xtst.XTestFakeMotionEvent(display, -1, x.value + offset, y.value, 0)
                        x11.XSync(display, 0)
                        next_move = time.monotonic() + 2
                    time.sleep(0.02)
                state = "on" if compositing else "off"
                deadline = time.monotonic() + 10
                while True:
                    active = dbus_call(gdbus, "/Compositor", "org.freedesktop.DBus.Properties.Get",
                                       "org.kde.kwin.Compositing", "active")
                    if ("true" in active) == compositing or time.monotonic() > deadline:
                        break
                    time.sleep(0.1)
                assert ("true" in active) == compositing, f"compositing should be {state}: {active!r}"
                if compositing:
                    loaded = dbus_call(gdbus, "/Effects", "org.kde.kwin.Effects.isEffectLoaded", "dwellclicker")
                    assert "true" in loaded, f"the ring effect isn't loaded: {loaded!r}"
                    # Halfway through a dwell the screen around the pointer must show the ring's arc.
                    x, y, child = C.c_int(), C.c_int(), C.c_ulong()
                    x11.XTranslateCoordinates(display, window, root, 100, 100, C.byref(x), C.byref(y), C.byref(child))
                    xtst.XTestFakeMotionEvent(display, -1, x.value, y.value, 0)
                    x11.XSync(display, 0)
                    time.sleep(0.2)
                    xtst.XTestFakeMotionEvent(display, -1, x.value + 20, y.value, 0)
                    x11.XSync(display, 0)
                    time.sleep(0.05 + DWELL_TIME / 2000)
                    ring = ring_pixels(x11, display, root, x.value + 20, y.value)
                    assert ring >= 40, f"no progress ring on screen mid-dwell: {ring} ring-coloured pixels"
                    print(f"PASS: progress ring drawn mid-dwell ({ring} ring-coloured pixels)")
                    # Moving while the ring animates restarts the dwell from inside a ring update.
                    end = time.monotonic() + 16
                    step = 0
                    while time.monotonic() < end:
                        assert wm.poll() is None, f"WM crashed moving during a dwell: exit {wm.returncode}"
                        time.sleep(0.07)
                        xtst.XTestFakeMotionEvent(display, -1, x.value + (step % 2) * 20, y.value + (step % 3) * 20, 0)
                        x11.XSync(display, 0)
                        step += 1
                    time.sleep(0.5)
                    assert wm.poll() is None, f"WM crashed moving during a dwell: exit {wm.returncode}"
                    print(f"PASS: moving during the ring animation is safe ({step} moves)")
                print(f"PASS: dwell click arrived with compositing {state}: {seen}")
            except BaseException:
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                if wm is not None and wm.poll() is None:
                    wm.terminate()
                    wm.wait(timeout=10)
                x11.XCloseDisplay(display)


def main():
    wm_path, gdbus = sys.argv[1:]
    x11, xtst = load_x11()
    run(wm_path, gdbus, True, x11, xtst)
    run(wm_path, gdbus, False, x11, xtst)


if __name__ == "__main__":
    main()
