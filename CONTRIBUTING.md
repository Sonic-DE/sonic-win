# Contributing to sonic-win

## What Needs Doing

You can open issues for sonic-win on our [issue tracker](https://github.com/Sonic-DE/sonic-win/issues).

## Where Stuff Is

Everything codewise for sonic-win itself is located in the `src` directory.

### Settings Pages / KCMs

All the settings pages for sonic-win found in System Settings are located in `src/kcmkwin`.

### Default Decorations

The Breeze decorations theme is not located in the sonic-win repository, and is in fact part of the [Breeze repository here](https://invent.kde.org/plasma/breeze), in `kdecoration`.

### Tab Switcher

The default visual appearance of the tab switcher is located in `src/tabbox/switchers`.

Other window switchers usually shipped by default are located in [Plasma Addons](https://invent.kde.org/plasma/kdeplasma-addons), located in the `kwin/windowswitchers` directory.

### Window Management

Most window management stuff (layouting, movement, properties, communication between client<->server) is defined in files ending with `client`, such as `x11client.cpp` and `xdgshellclient.cpp`.

### Window Effects

Window effects are located in `src/plugins`, one effect plugin per folder.  Folder `src/plugins/private` contains the plugin (`org.kde.kwin_x11.private.effects`) that exposes layouting properties and `WindowHeap.qml` for QML effects.  Not everything here is an effect as exposed in the configuration UI, such as the colour picker in `src/plugins/colorpicker`.

Of note, the Effects QML engine is shared with the Scripting components (see `src/scripting`).

### Scripting API

Many objects in sonic-win are exposed directly to the scripting API; scriptable properties are marked with Q_PROPERTY and functions that scripts can invoke on them.

Other scripting stuff is located in `src/scripting`.

## Conventions

### Coding Conventions

We follow sonic-win's coding conventions which are located in the [coding-conventions](doc/coding-conventions.md) document.

We additionally follow [KDE's Frameworks Coding Style](https://community.kde.org/Policies/Frameworks_Coding_Style).

### Commits

We usually use this convention for commits in sonic-win:

```git
component/subcomponent: Do a thing

This is a body of the commit message,
elaborating on why we're doing thing.
```

While this isn't a hard rule, it's appreciated for easy scanning of commits by their messages.

## Contributing

sonic-win uses Sonic DE's sonic-win [GitHub repository](https://github.com/Sonic-DE/sonic-win) for submitting code.

It's just a matter of forking the repository and then doing a pull request with your changes.

## Running sonic-win From Source

sonic-win uses CMake like most SonicDE projects, so you can build it like this:

```bash
mkdir _build
cd _build
cmake ..
make
```

Once built, you can either install it over your system sonic-win (not recommended) or run it from the build directory directly.

Running it from your build directory looks like this:

```bash
# from the root of your build directory

source prefix.sh
cd bin

# or for x11, replaces current kwin instance:

env QT_PLUGIN_PATH="$(pwd)":"$QT_PLUGIN_PATH" ./kwin_x11 --replace

```

QT_PLUGIN_PATH tells Qt to load sonic-win's plugins from the build directory, and not from your system sonic-win.

The dbus-run-session is needed to prevent the nested sonic-win instance from conflicting with your session sonic-win instance when exporting objects onto the bus, or with stuff like global shortcuts.

```bash
kdesrc-build plasma-workspace
# assuming the root directory for kdesrc-build is ~/kde
bash ~/kde/build/plasma-workspace/login-sessions/install-sessions.sh
```

Then you can select the develop session in the sddm login screen.

You can look up the current boot kwin log via `journalctl --user-unit plasma-kwin_x11 --boot 0`.

## Using A Debugger

Trying to attach a debugger to a running sonic-win instance from within itself will likely be the last thing you do in the session, as sonic-win will freeze until you resume it from your debugger, which you need sonic-win to interact with.

Instead, either attach a debugger to a nested sonic-win instance or debug over SSH.

## Tests

sonic-win has a series of unit tests and integration tests that ensure everything is running as expected.

If you're adding substantial new code, it's expected that you'll write tests for it to ensure that it's working as expected.

If you're fixing a bug, it's appreciated, but not expected, that you add a test case for the bug you fix.
