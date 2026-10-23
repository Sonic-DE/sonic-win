# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the real WM, D-Bus interface and gamma ramps on an isolated Xvfb."""

import ctypes as C
import ctypes.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


class Resources(C.Structure):
    _fields_ = [
        ("timestamp", C.c_ulong), ("configTimestamp", C.c_ulong),
        ("ncrtc", C.c_int), ("crtcs", C.POINTER(C.c_ulong)),
        ("noutput", C.c_int), ("outputs", C.POINTER(C.c_ulong)),
        ("nmode", C.c_int), ("modes", C.c_void_p),
    ]


class Gamma(C.Structure):
    _fields_ = [("size", C.c_int), ("red", C.POINTER(C.c_ushort)),
                ("green", C.POINTER(C.c_ushort)), ("blue", C.POINTER(C.c_ushort))]


def wait_for(check, description, wm, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if wm.poll() is not None:
            raise AssertionError(f"WM exited with {wm.returncode}: {description}")
        value = check()
        if value:
            return value
        time.sleep(0.03)
    raise AssertionError(f"Timed out: {description}")


def main():
    wm_path, gdbus = sys.argv[1:]
    x11 = C.CDLL(ctypes.util.find_library("X11"))
    randr = C.CDLL(ctypes.util.find_library("Xrandr"))
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XDefaultRootWindow.argtypes = [C.c_void_p]
    x11.XDefaultRootWindow.restype = C.c_ulong
    x11.XCloseDisplay.argtypes = [C.c_void_p]
    randr.XRRGetScreenResourcesCurrent.argtypes = [C.c_void_p, C.c_ulong]
    randr.XRRGetScreenResourcesCurrent.restype = C.POINTER(Resources)
    randr.XRRFreeScreenResources.argtypes = [C.POINTER(Resources)]
    randr.XRRGetCrtcGamma.argtypes = [C.c_void_p, C.c_ulong]
    randr.XRRGetCrtcGamma.restype = C.POINTER(Gamma)
    randr.XRRFreeGamma.argtypes = [C.POINTER(Gamma)]
    display = x11.XOpenDisplay(None)
    assert display, "Xvfb must be running"
    resources = randr.XRRGetScreenResourcesCurrent(display, x11.XDefaultRootWindow(display))
    assert resources and resources.contents.ncrtc > 0
    crtc = resources.contents.crtcs[0]
    randr.XRRFreeScreenResources(resources)

    def gamma():
        ramp = randr.XRRGetCrtcGamma(display, crtc)
        assert ramp and ramp.contents.size > 1
        index = ramp.contents.size - 1
        result = tuple(channel[index] for channel in (ramp.contents.red, ramp.contents.green, ramp.contents.blue))
        randr.XRRFreeGamma(ramp)
        return result

    def call(method, *args, path="/org/sonicde/SoftwareBrightness"):
        result = subprocess.run(
            [gdbus, "call", "--session", "--dest", "org.kde.KWin", "--object-path", path,
             "--method", method, *map(str, args)], capture_output=True, text=True, timeout=5,
        )
        return result.stdout.strip() if result.returncode == 0 else ""

    def set_brightness(name, value, dimming=1.0):
        assert call("org.sonicde.SoftwareBrightness.SetBrightness", name, value, dimming) == "(true,)"

    wm = None
    with tempfile.TemporaryDirectory(prefix="sonic-brightness-") as temp:
        environment = os.environ.copy()
        environment.update(QT_QPA_PLATFORM="xcb", QT_QPA_PLATFORMTHEME="generic", QT_STYLE_OVERRIDE="Fusion",
                           QT_PLUGIN_PATH=str(Path(wm_path).parent), XDG_CONFIG_HOME=temp,
                           XDG_CACHE_HOME=temp, KWIN_COMPOSE="N")
        Path(temp, "kwinrc").write_text("[Compositing]\nEnabled=false\n[Plugins]\nnightlightEnabled=true\n")
        with tempfile.TemporaryFile(mode="w+") as log:
            def start():
                process = subprocess.Popen([wm_path], env=environment, stdout=log, stderr=log)
                return process

            try:
                wm = start()
                info = wait_for(lambda: call("org.sonicde.SoftwareBrightness.Displays"), "brightness service", wm)
                match = re.search(r"'([^']+)':", info)
                assert match, info
                name = match.group(1)
                set_brightness(name, 5000)
                wait_for(lambda: all(abs(value - 32768) <= 2 for value in gamma()), "50% native gamma", wm)
                set_brightness(name, 5000, 0.5)
                wait_for(lambda: all(abs(value - 16384) <= 2 for value in gamma()), "independent idle dimming", wm)
                set_brightness(name, 5000)
                # Preview an actual Night Light transition while brightness is held at 50%.
                wait_for(lambda: call("org.kde.KWin.NightLight.preview", 3500, path="/org/kde/KWin/NightLight"),
                         "Night Light preview", wm)
                wait_for(lambda: gamma()[0] > gamma()[2] * 1.3 and abs(gamma()[0] - 32768) <= 2,
                         "Night Light and brightness composition", wm)
                set_brightness(name, 2500)
                wait_for(lambda: gamma()[0] > gamma()[2] * 1.3 and abs(gamma()[0] - 16384) <= 2,
                         "slider keeps Night Light", wm)
                assert call("org.sonicde.SoftwareBrightness.SetBrightness", name, 0, 1.0) == "(false,)"

                # Restart the WM with the same configuration: user brightness survives,
                # temporary dimming and preview colour temperature do not.
                set_brightness(name, 5000, 0.5)
                wm.terminate()
                wm.wait(timeout=10)
                wm = start()
                wait_for(lambda: call("org.sonicde.SoftwareBrightness.Displays"), "restarted service", wm)
                wait_for(lambda: all(abs(value - 32768) <= 2 for value in gamma()), "persisted brightness after restart", wm)
                client = os.environ.get("SOFTWARE_BRIGHTNESS_CLIENT_TEST")
                if client:
                    subprocess.run([client, "realCompositor"], env=environment, check=True, timeout=20)
                set_brightness(name, 10000)
                wait_for(lambda: all(value == 65535 for value in gamma()), "full brightness restoration", wm)
                print("PASS: X11 gamma, idle dimming, Night Light composition, persistence, restoration")
            except BaseException:
                print(f"Last gamma endpoints: {gamma()}", file=sys.stderr)
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                if wm is not None and wm.poll() is None:
                    wm.terminate()
                    wm.wait(timeout=10)
                x11.XCloseDisplay(display)


if __name__ == "__main__":
    main()
