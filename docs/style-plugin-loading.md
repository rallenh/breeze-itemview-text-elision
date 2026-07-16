# How Qt5 Applications Load breeze5.so

## The Chain

Qt5 applications using Breeze do not link against `breeze5.so` directly.
The style is loaded at runtime via Qt's plugin system. On a KDE session the
chain is:

```
KDE session startup
  └─ sets QT_QPA_PLATFORMTHEME=kde (or the app inherits it)
        └─ Qt loads the KDE platform theme plugin (libqt5platformtheme_kde.so)
              └─ reads ~/.config/kdeglobals  →  widgetStyle=Breeze
                    └─ QStyleFactory::create("Breeze")
                          └─ dlopen()s breeze5.so from QT_PLUGIN_PATH
```

The key: **the application never names Breeze**. It calls
`QApplication::style()` and gets back whatever `QStyleFactory` instantiated,
which is whatever `kdeglobals` configured. This is why all three affected
applications (KeePassXC, SMPlayer, GoldenDict) exhibit the same bug — they
share a common style plugin, not common application code.

## What breeze5.so Is

`breeze5.so` is a Qt5 style plugin built from `kstyle/` in the
`plasma/breeze` repository. It is packaged in `plasma-breeze-qt5` on Fedora.

```
/usr/lib64/qt5/plugins/styles/breeze5.so   ← the plugin Qt loads
```

The same source (`kstyle/breezestyle.cpp`) is compiled again for Qt6 as
`breeze6.so`, installed at:

```
/usr/lib64/qt6/plugins/styles/breeze6.so
```

Both `.so` files carry the same `SE_ItemViewItemText` code path and the
same bug. The Qt5 and Qt6 plugins are independent runtime loads — patching
one does not affect the other.

## Why KDE Apps Are Also Affected

Applications that use KDE Frameworks (e.g., KDE's own apps, apps that link
against `KF5::WidgetsAddons`, `KF5::IconThemes`, etc.) still load Breeze
through the same `QStyleFactory` path. Linking against KDE Frameworks does
not change which style is active — it adds KDE-specific widgets, but the
style applied to `QAbstractItemView` and its subclasses remains the one
configured in `kdeglobals`.

## SMPlayer: Why It Normally Avoids the Bug

SMPlayer defaults to `Style: Fusion` in its Preferences, which is why the
bug does not affect it by default. Fusion uses `QCommonStyle::subElementRect`
for `SE_ItemViewItemText` without any horizontal narrowing (`diff=0`). Only
when the user (or a packager) changes the style to Breeze does the elision
appear.

This is also why SMPlayer was chosen as a control: switching between Fusion
and Breeze in the same running process isolates the defect to the style
plugin, not the application code.

## KeePassXC: Why It Always Shows the Bug on KDE

KeePassXC does not set a style. Its "Automatic" theme (the default,
`View → Theme → Automatic`) instructs the application to use the platform
style, which on a KDE session is Breeze. There is no app-level override to
fall back to Fusion the way SMPlayer does.

The "Classic (Platform-native)" KeePassXC theme also routes through Breeze
on KDE — it bypasses KeePassXC's own icon/color theming but not Qt's style
system.

## GoldenDict: Same as KeePassXC

GoldenDict has no style selector. It uses whatever the platform theme
provides, which on KDE is Breeze. `View → Theme → Automatic` is the
operative setting and is the default.

## Verifying Which .so Is Loaded

```bash
# Confirm the installed version
rpm -q plasma-breeze-qt5

# See the actual .so being loaded by a running process (replace PID)
cat /proc/<PID>/maps | grep breeze
```

The [`run-clean.sh`](../probe/run-clean.sh) wrapper in [`probe/`](../probe/) prints the installed RPM version
before launching the application, tying each screenshot to a specific
build of `breeze5.so`.
