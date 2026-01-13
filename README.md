# sonic-win

## KWin/X11 with ports from KWin/Wayland, bug fixes, and other improvements

sonic-win is an X11 window manager and a compositing manager. Its primary usage is in conjunction with a Desktop Shell (e.g. [sonic-desktop](https://github.com/Sonic-DE/sonic-desktop)). sonic-win is designed to stay out of the way; users should not notice that they use a window manager at all. Nevertheless sonic-win provides a steep learning curve for advanced features, which are available, if they do not conflict with the primary mission. sonic-win does not have a dedicated targeted user group, but follows the targeted user group of the Desktop Shell using sonic-win as it's window manager.

## Why stay with X11?

On X11 there is [a working implementation](https://github.com/guiodic/material-decoration) for Locally Integrated Menus. Inertial scrolling works too, even under Wine. The scrolling in LibreOffice/Qt is more fluent, and some Chromium functionality, such as drag & drop, is more reliable than on Wayland. Global hotkeys are working for, e.g., push to talk in Telegram and the recording toggle in OBS Studio. Windows get restored at the positions they were closed, especially between different sessions. Applications like games know which screen is the primary one. You can also record your screen in remote desktop applications such as TeamViewer. There are some other minor aspects that just work and lead to an overall pleasant desktop experience.

KWin for X11 was created and for a long time maintained by the [KDE](https://kde.org) developers. Unfortunately, however, the KDE developers decided to abandon X11. In the wake of these events, KWin/X11 has been patched in 2025 by [guiodic](https://github.com/guiodic) at [guiodic/kwin-x11-improved](https://github.com/guiodic/kwin-x11-improved). The SonicDE project is here to pick up the baton, fix bugs, and make improvements to KWin/X11.

You may want to check out [guiodic's Plasma X11 improved guide](https://gist.github.com/guiodic/2bcc8f2f126d14b1f8a439f644fdc2c9) to get a better Plasma X11 experience. Note: Many instructions here are specific to intel video cards.

There is also a [critical comparison of X11 and Wayland by probonopd](https://gist.github.com/probonopd/9feb7c20257af5dd915e3a9f2d1f2277).

## sonic-win is not

* a standalone window manager (c.f. openbox, i3) and does not provide any functionality belonging to a Desktop Shell.
* a replacement for window managers designed for use with a specific Desktop Shell (e.g. GNOME Shell)
* a minimalistic window manager
* designed for use without compositing or for X11 network transparency, though both are possible.

## Contributing to sonic-win

We appreciate your interest in contributing! Please refer to the [CONTRIBUTING document](CONTRIBUTING.md) for everything you need to get started. To report a bug, please use the sonic-win bug tracker at [Issues · Sonic-DE/sonic-win](https://github.com/Sonic-DE/sonic-win/issues).

## Getting in contact

We'd love to hear from you on one of our channels. To get end-user support, please also check your distribution's chat or forum.

<img src="./.github/icons/bluesky.svg">&nbsp;[Bluesky](https://bsky.app/profile/sonicdesktop.bsky.social)&nbsp; <img src="./.github/icons/discord.svg">&nbsp;[Discord](https://discord.gg/cNZMQ62u5S) &nbsp; <img src="./.github/icons/mastodon.svg">&nbsp;[Mastodon](https://mastodon.social/@sonicdesktop) &nbsp; <img src="./.github/icons/matrix.svg">&nbsp;[Matrix](https://matrix.to/#/#sonicdesktop:matrix.org) &nbsp; <img src="./.github/icons/oftc.svg">&nbsp;[OFTC IRC](https://webchat.oftc.net/?channels=sonicde%2Csonicde-devel%2Csonicde-dist&uio=MT11bmRlZmluZWQb1) &nbsp; <img src="./.github/icons/telegram.svg">&nbsp;[Telegram](https://t.me/sonic_de) &nbsp; <img src="./.github/icons/x.svg">&nbsp;[X (Twitter)](https://x.com/SonicDesktop)
