# KWin/X11 with ports from kwin-wayland, bug fixes, and maybe other improvements

This is a fork of [guiodic](https://github.com/guiodic/kwin-x11-improved)'s repository with the full git tree mirrored instead of using patches.

KWin/X11 is an X11 window manager and a compositing manager. Its primary usage is in conjunction with a Desktop Shell (e.g. KDE Plasma Desktop). KWin/X11 is designed to go out of the way; users should not notice that they use a window manager at all. Nevertheless KWin/X11 provides a steep learning curve for advanced features, which are available, if they do not conflict with the primary mission. KWin does not have a dedicated targeted user group, but follows the targeted user group of the Desktop Shell using KWin/X11 as it's window manager.

## Why still on X11?

With each release of a new version of Plasma many of us try to make the switch to Wayland, only to quickly revert back to X11.

Here are the reasons:

1. The first is perhaps personal and concerns the Locally Integrated Menu. I use the [material decoration](https://github.com/guiodic/material-decoration) that implements LIM. I saw that KDE devs are working on an upstream implementation, but both implementations won't work for GTK apps. They say that a [special Wayland protocol](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/52) is needed, as usual. So in all likelihood it will never be solved, unless KDE rewrites the GTK plugin to use KDE's (private) protocol.

2. The second concerns the lack of inertial scrolling. Xorg allows this at the server level, with Synaptics driver, while in Wayland each application has to do it on its own. Again, this will take many years. It will probably never happen for applications under Wine, which I depend on for work, as Linux has no decent PDF editor. But apart from that, using Okular without inertial scrolling is painful. Recently, KDE developers said they had implemented inertial scrolling for QtQuick apps, but it does not actually work.

3. Libreoffice+QT (or KF5/6) on Wayland has had a bug for years that makes scrolling slow and jerky to an intolerable level. This is partly due to the fact that the Qt backend for Wayland is [much slower](https://bugreports.qt.io/browse/QTBUG-139231) than the one for X11.

4. Chromium still has numerous problems under Wayland such as drag&drop not always working. This is also due to the different interpretations of the same protocol among different compositors (such as Mutter in GNOME and Kwin in KDE).

5. Of course, one cannot forget the problem of restoring windows in the position in which they were closed the last time, especially between different sessions, which will probably take many more years to solve. A very basic feature still lacking in Wayland.

6. Global hotkey support breaks things like push to talk in various applications such as Telegram or recording toggle in OBS Studio.

7. Screen recording for remote desktop applications such as Team Viewer are broken.
    
8. The inability for an application to know which screen is the primary screen ([very annoying for games](https://www.youtube.com/watch?v=pLzxP4WFe5U)).

9. After that there are minor annoyances (unreliable thumbnails in Plasma, among them), but which still make the overall experience disappointing.

These, and other minor other deficiencies, severely impact people's work.

A few of these problems are solved by launching apps with Xwayland. At this point, you might as well use X11.

Unfortunately, however, the KDE developers are abandoning X11. Improvements are rejected and bugs are not fixed.

This is why I found myself forced to open this repository that is a patchset for kwin_x11. If you want to contribute, prepare an MR or write a bug report. Thank you in advance!

If you want an improved experience with kwin_x11 check out [guiodic's guide](https://gist.github.com/guiodic/2bcc8f2f126d14b1f8a439f644fdc2c9).

For more on Wayland's problems [see also this](https://gist.github.com/probonopd/9feb7c20257af5dd915e3a9f2d1f2277=).

## KWin is not

* a standalone window manager (c.f. openbox, i3) and does not provide any functionality belonging to a Desktop Shell.
* a replacement for window managers designed for use with a specific Desktop Shell (e.g. GNOME Shell)
* a minimalistic window manager
* designed for use without compositing or for X11 network transparency, though both are possible.

## Contributing to KWin

Please refer to the [contributing document](CONTRIBUTING.md) for everything you need to know to get started contributing to KWin.

## Contacting the Sonic DE development team

* IRC: #sonicde on irc.libera.chat
* Telegram: (https://t.me/sonic_de)

## Support

### Application Developer

If you are an application developer having questions regarding windowing systems (X11) please do not hesitate to contact us. Preferably through our Telegram.

### End user

Please contact the support channels of your Linux distribution for user support. The Sonic DE development team does not provide end user support.

## Reporting bugs

Please use [Sonic De's issue tracker](https://github.com/Sonic-DE/kwin-x11-sonic/issues) and report your issue.
