# SPDX-License-Identifier: GPL-2.0-or-later
"""The compositor must not claim to run while another client redirects the windows, and must start after it's gone."""

import ctypes as C
import ctypes.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

REDIRECT_MANUAL = 1


def wait_for(check, description, wm, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if wm.poll() is not None:
            raise AssertionError(f"WM exited with {wm.returncode}: {description}")
        if check():
            return
        time.sleep(0.05)
    raise AssertionError(f"Timed out: {description}")


def main():
    wm_path, gdbus = sys.argv[1:]
    x11 = C.CDLL(ctypes.util.find_library("X11"))
    composite = C.CDLL(ctypes.util.find_library("Xcomposite"))
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XDefaultRootWindow.argtypes = [C.c_void_p]
    x11.XDefaultRootWindow.restype = C.c_ulong
    x11.XSync.argtypes = [C.c_void_p, C.c_int]
    x11.XCloseDisplay.argtypes = [C.c_void_p]
    composite.XCompositeRedirectSubwindows.argtypes = [C.c_void_p, C.c_ulong, C.c_int]

    holder = x11.XOpenDisplay(None)
    assert holder, "Xvfb must be running"
    composite.XCompositeRedirectSubwindows(holder, x11.XDefaultRootWindow(holder), REDIRECT_MANUAL)
    x11.XSync(holder, 0)

    def active():
        result = subprocess.run(
            [gdbus, "call", "--session", "--dest", "org.kde.KWin", "--object-path", "/Compositor",
             "--method", "org.freedesktop.DBus.Properties.Get", "org.kde.kwin.Compositing", "active"],
            capture_output=True, text=True, timeout=5,
        )
        return result.stdout.strip() if result.returncode == 0 else ""

    wm = None
    with tempfile.TemporaryDirectory(prefix="sonic-redirect-") as temp:
        environment = os.environ.copy()
        environment.update(QT_QPA_PLATFORM="xcb", QT_QPA_PLATFORMTHEME="generic", QT_STYLE_OVERRIDE="Fusion",
                           QT_PLUGIN_PATH=os.pathsep.join(filter(None, [str(Path(wm_path).parent),
                                                                        os.environ.get("QT_PLUGIN_PATH")])),
                           XDG_CONFIG_HOME=temp, XDG_CACHE_HOME=temp, LIBGL_ALWAYS_SOFTWARE="1", KDE_DEBUG="1")
        Path(temp, "kwinrc").write_text("[Compositing]\nEnabled=true\n")
        with tempfile.TemporaryFile(mode="w+") as log:
            try:
                wm = subprocess.Popen([wm_path], env=environment, stdout=log, stderr=log)
                wait_for(active, "compositor on D-Bus", wm)
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    assert "true" not in active(), "compositing reported active while another client redirects"
                    time.sleep(0.1)
                x11.XCloseDisplay(holder)
                holder = None
                wait_for(lambda: "true" in active(), "compositing after the other client disconnected", wm)
                print("PASS: compositing waits for the other client, then starts")
            except BaseException:
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                if wm is not None and wm.poll() is None:
                    wm.terminate()
                    wm.wait(timeout=10)
                if holder:
                    x11.XCloseDisplay(holder)


if __name__ == "__main__":
    main()
