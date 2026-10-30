# Sonic Win Release Notes

Sonic Win is SonicDE's window manager and compositor for X11: it places, moves and decorates your windows and draws the smooth effects you see on screen.

## Unreleased

- Window Decorations' "Configure Titlebar…" page now also sets the titlebar's transparency: opacity for active and inactive windows, blur, opaque when maximized, matching the app colour, and whether toolbars and menu bars under the titlebar share it. These settings now work with any theme and apply straight away, in modern apps too. Its new Exceptions section changes these transparency settings for chosen applications.
- The Blur desktop effect can now also blur behind menus made see-through by the Translucency effect, with its own blur, noise and saturation strength. The existing settings are now grouped as "Blur windows".
- Fixed the Blur effect's saturation setting having no effect.
- Screen brightness can now be adjusted in software on any X11 display, works together with Night Light, and is remembered between sessions.
- The Sonic Screenies screenshot tool no longer shows up fading or zooming in its own screenshots.
- Fixed translucent windows turning fully opaque in screenshots and screen captures.
- Desktop and shell integration now uses SonicDE's own names throughout, so other SonicDE components recognise the window manager correctly.
- Behind the scenes: the NVIDIA settings profile now installs relative to the build's install location, so building without administrator rights no longer fails.

## 6.8.0.2 (2026-10-24)

- The Sonic Screenies screenshot tool no longer shows up fading or zooming in its own screenshots.
- Fixed translucent windows turning fully opaque in screenshots and screen captures.

## 6.8.0.1 (2026-10-23)

- Screen brightness can now be adjusted in software on any X11 display, works together with Night Light, and is remembered between sessions.

## 6.8.0 (2026-10-20)

- Fixed displays on some NVIDIA systems repeatedly switching modes because of slightly-off refresh rates.
- Animations are smoother thanks to more precise frame timing.
- Variable refresh rate (adaptive sync) and the NVIDIA on-screen overlay are now switched off for the desktop itself on NVIDIA and Mesa drivers, avoiding flicker and stutter.
- Fixed jumbled log messages when several parts of the window manager write to the log at once.
- Behind the scenes: new automated tests for frame timing, plus release and version updates.

## 6.7.5.3 (2026-09-23)

- Variable refresh rate (adaptive sync) and the NVIDIA on-screen overlay are now switched off for the desktop itself on NVIDIA and Mesa drivers, avoiding flicker and stutter.

## 6.7.5.2 (2026-09-22)

- Animations are smoother thanks to more precise frame timing.
- Fixed jumbled log messages when several parts of the window manager write to the log at once.
- Behind the scenes: new automated tests for frame timing.

## 6.7.5.1 (2026-09-18)

- Fixed displays on some NVIDIA systems repeatedly switching modes because of slightly-off refresh rates.

## 6.7.5 (2026-09-12)

- Fixed wrong refresh rates being calculated for very high-resolution or high-refresh displays.
- Translations now show up correctly, including in the window that offers to close a frozen application.
- Overview, window view and the tiling editor no longer need Plasma or old Qt 5 support libraries, making SonicDE lighter to install.
- Behind the scenes: release preparation.

## 6.7.4.3 (2026-08-20)

- The default wallpaper is now SonicDE's Silver wallpaper everywhere it was still missing.

## 6.7.4.2 (2026-08-12)

- The window manager is now called sonic-win, with its own name in system services and process lists.

## 6.7.4.1 (2026-08-12)

- Dialogs that ask to open at a specific spot over their parent window now appear exactly where the application intended.

## 6.7.4 (2026-08-01)

- Fixed a crash when the window manager tried to restart itself with a very long command.

## 6.7.3.3 (2026-08-04)

- More of the look and feel now uses SonicDE's Silver theme instead of Breeze.

## 6.7.3.2 (2026-08-02)

- The SonicDE logo now replaces the old KDE logo throughout.

## 6.7.3.1 (2026-08-01)

- Silver is now the base and default theme, replacing Breeze.

## 6.7.3 (2026-07-14)

- Behind the scenes: restored the code formatting rules.

## 6.7.2 (2026-07-02)

- Behind the scenes: removed leftover Wayland-only code and tidied developer tool settings.

## 6.7.1 (2026-06-23)

No SonicDE-specific changes in this release; it brings the package up to date with upstream.

## 6.7.0 (2026-06-16)

- Night Light is now an optional part of the window manager, so slimmer systems can leave it out.
- Behind the scenes: fixed a build warning.

## 6.6.5.1 (2026-05-24)

- Fixed a crash in the window switcher when some of its alternative shortcuts were left unassigned.

## 6.6.5 (2026-05-12)

No SonicDE-specific changes in this release; it brings the package up to date with upstream.

## 6.6.4.1 (2026-04-09)

- Themes and other add-ons can once again recognise the window manager, fixing setups where they did not load properly.

## 6.6.4 (2026-04-07)

- Behind the scenes: removed unused code.

## 6.6.3.1 (2026-03-30)

- New windows now open on the right screen and at the position the application asked for in almost all cases.
- Fixed the translucency effect not applying to the right window when animations started or stopped.
- Behind the scenes: code tidying and developer tool cleanup.

## 6.6.3 (2026-03-18)

- Behind the scenes: updated the contributor guide.

## 6.6.2.1 (2026-03-06)

- Behind the scenes: added an X11 description of the input device interface used by other components, and code safety improvements for display information.

## 6.6.2 (2026-03-03)

No SonicDE-specific changes in this release; it brings the package up to date with upstream.

## 6.6.1 (2026-02-24)

- Behind the scenes: build system fixes.

## 6.6.0 (2026-02-12)

- Sonic Win is now a pure X11 window manager: all remaining Wayland code is gone, making it leaner and easier to maintain.
- Fixed memory leaks in the window switcher.
- Fixed a graphics setup mistake that could cause rendering problems with some effects.
- The mouse mark effect now keeps its freehand and arrow drawing shortcuts separate, so both work as expected.
- Behind the scenes: many code-quality and safety fixes, updates for newer Qt versions, quieter builds, new testing and memory-checking options, and better debug messages in development builds.

## 6.5.5 (2026-01-14)

- Behind the scenes: documentation updates, removal of Wayland-only tests and developer tool settings.

## 6.5.4 (2025-12-10)

- Fixed possible glitches or crashes when the screen synchronisation helpers shut down.
- Behind the scenes: README and contributor guide updates.

## 6.5.3.4 (2025-11-11)

- Behind the scenes: a small build fix and a README formatting fix.

## 6.5.3.3 (2025-11-30)

- Debugging effects such as the frame-rate and repaint overlays are now tucked away in the debug console, where they are listed first, and no longer clutter the regular effects settings.
- Disconnected monitors are no longer treated as active screens.
- The mouse mark effect now uses the correct modifier key for drawing arrows.
- Behind the scenes: build cleanups, removal of leftover global shortcut code, and documentation updates.

## 6.5.3.2 (2025-11-29)

- A refreshed Sonic Win logo.
- Behind the scenes: removed leftovers of the old contrast effect.

## 6.5.3 (2025-11-18)

- The mouse mark effect now uses the correct modifier key for drawing arrows.

## 6.5.2 (2025-11-04)

- Fixed odd flashing frames when a blurred window fades in on top of another one.

## 6.5.1 (2025-10-28)

- Debugging effects such as the frame-rate and repaint overlays are now tucked away in the debug console, where they are listed first, and no longer clutter the regular effects settings.
- Behind the scenes: removed an outdated keyboard handling function.

## 6.4.5.2 (2025-10-14)

- Two screen-change fixes from the previous release were rolled back after they caused problems.
- Behind the scenes: README updates.

## 6.4.5.1 (2025-10-03)

- Behind the scenes: removed a test for a module that is no longer included.

## 6.4.5 (2025-09-22)

- The first SonicDE release of the window manager: a dedicated X11 window manager and compositor, separated from its Wayland counterpart so it can be installed and configured on its own.
- Settings pages for window decorations, effects, virtual desktops, window behaviour, window rules, screen edges, scripts and the window switcher are now dedicated to the X11 desktop.
- Wayland-only features and settings that do not apply to an X11 desktop have been removed for a simpler, lighter package.
- Translations now load correctly across the window switcher, effects and their settings pages.
- Disconnected monitors are no longer treated as active screens.
- Screen edges no longer misbehave after a screen layout change, and the window manager no longer loops with the screen locker when nothing has changed.
- A shiny new icon.
- Behind the scenes: renamed internal libraries and files to avoid clashing with other window managers, cleaned up dependencies, removed the old global shortcut service, updated the README, and build fixes for newer Qt.
