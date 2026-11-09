# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""After an XInput hierarchy change, the window manager must still exit cleanly when replaced."""

import ctypes as C
import ctypes.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

XI_ADD_MASTER = 1


class XIAddMasterInfo(C.Structure):
    _fields_ = [("type", C.c_int), ("name", C.c_char_p), ("send_core", C.c_int), ("enable", C.c_int)]


def has_owner(gdbus, name):
    result = subprocess.run([gdbus, "call", "--session", "--dest", "org.freedesktop.DBus", "--object-path",
                             "/org/freedesktop/DBus", "--method", "org.freedesktop.DBus.NameHasOwner", name],
                            capture_output=True, text=True, timeout=5)
    return "true" in result.stdout


def main():
    wm_path, gdbus = sys.argv[1:]
    x11 = C.CDLL(ctypes.util.find_library("X11"))
    xi = C.CDLL(ctypes.util.find_library("Xi"))
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XSync.argtypes = [C.c_void_p, C.c_int]
    x11.XCloseDisplay.argtypes = [C.c_void_p]
    xi.XIChangeHierarchy.argtypes = [C.c_void_p, C.c_void_p, C.c_int]

    with tempfile.TemporaryDirectory(prefix="sonic-hierarchy-") as temp:
        home = Path(temp)
        runtime = home / "runtime"
        runtime.mkdir(mode=0o700)
        for name in ("config", "cache", "data", "state"):
            (home / name).mkdir()
        (home / "config" / "kwinrc").write_text("[Compositing]\nEnabled=false\n")
        environment = os.environ.copy()
        environment.update(QT_QPA_PLATFORM="xcb", QT_QPA_PLATFORMTHEME="generic", HOME=temp,
                           XDG_CONFIG_HOME=str(home / "config"), XDG_CACHE_HOME=str(home / "cache"),
                           XDG_DATA_HOME=str(home / "data"), XDG_STATE_HOME=str(home / "state"),
                           XDG_RUNTIME_DIR=str(runtime), KDE_DEBUG="1",
                           QT_PLUGIN_PATH=os.pathsep.join(filter(None, [str(Path(wm_path).parent),
                                                                        os.environ.get("QT_PLUGIN_PATH")])))
        display = x11.XOpenDisplay(None)
        assert display, "Xvfb must be running"
        first = second = None
        with tempfile.TemporaryFile(mode="w+") as log:
            try:
                first = subprocess.Popen([wm_path], env=environment, stdout=log, stderr=log)
                deadline = time.monotonic() + 20
                while not has_owner(gdbus, "org.kde.KWin"):
                    assert first.poll() is None, f"WM exited with {first.returncode}"
                    assert time.monotonic() < deadline, "Timed out waiting for the window manager"
                    time.sleep(0.05)
                # Adding a master device, as plugging one in does, makes Qt's xcb platform
                # delete every registered input device it doesn't know.
                info = XIAddMasterInfo(XI_ADD_MASTER, b"hierarchy probe", 1, 1)
                assert xi.XIChangeHierarchy(display, C.byref(info), 1) == 0, "XIChangeHierarchy failed"
                x11.XSync(display, 0)
                time.sleep(1)
                assert first.poll() is None, f"WM exited after the hierarchy change: {first.returncode}"

                second = subprocess.Popen([wm_path, "--replace"], env=environment, stdout=log, stderr=log)
                code = first.wait(timeout=20)
                assert code == 0, f"replaced window manager exited with {code}, expected a clean exit"
                print("PASS: the replaced window manager exits cleanly after a hierarchy change")
            except BaseException:
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                for wm in (first, second):
                    if wm is not None and wm.poll() is None:
                        wm.terminate()
                        wm.wait(timeout=10)
                x11.XCloseDisplay(display)


if __name__ == "__main__":
    main()
