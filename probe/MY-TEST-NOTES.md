# My Test Procedure Notes

My personal reference notes for running the test harness, capturing artifacts
(probe output, screenshots), and evaluating each build of `plasma-breeze` as a
potential fix for the `SE_ItemViewItemText` elision issue.

## What I am testing

Patch 2 keeps the 2px `itemViewItemMargins` edge inset on each side but removes
`ItemView_ItemPaddingWidth` (2px/side). Net: text rect is 4px narrower than the
cell (2px each side) instead of 8px narrower (Patch 1 = 0px narrower, bug = 8px).

Decision criterion:
- If any test app still elides labels → Patch 1 is the MR lead, Patch 2 is the
  alternative presented for discussion.
- If all apps are clean at 4px → Patch 2 is the MR lead.

---

## Installed RPM versions

| Build | Release      | Patch            | Status   |
|-------|--------------|------------------|----------|
| .2    | 6.7.3-1.fc43.2 | Patch 1 (0px)  | verified |
| .3    | 6.7.3-1.fc43.3 | Patch 2 (4px)  | testing  |

Revert to .2:
```
sudo dnf downgrade \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-6.7.3-1.fc43.2.x86_64.rpm \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-common-6.7.3-1.fc43.2.noarch.rpm \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-qt5-6.7.3-1.fc43.2.x86_64.rpm \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-qt6-6.7.3-1.fc43.2.x86_64.rpm
```

Upgrade to .3:
```
sudo dnf install \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-6.7.3-1.fc43.3.x86_64.rpm \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-common-6.7.3-1.fc43.3.noarch.rpm \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-qt5-6.7.3-1.fc43.3.x86_64.rpm \
  ~/rpmbuild/RPMS/x86_64/plasma-breeze-qt6-6.7.3-1.fc43.3.x86_64.rpm
```

**Reboot required after each install/downgrade** — breeze5.so and breeze6.so
are loaded at app startup; dnf does not hot-swap them.

---

## Test 1 — Qt5 probe (numeric)

Run from `probe/` directory. Expect `diff=4` for Breeze at all column widths.

```
cd /home/allen/Workspace/breeze-itemview-text-elision/probe
./qt5-style-elide-test
```

Expected output for Breeze row:
```
Breeze    col  80px: diff=4   col 120px: diff=4   col 200px: diff=4
```

Reference (Patch 1 / .2):
```
Breeze    col  80px: diff=0   col 120px: diff=0   col 200px: diff=0
```

Reference (bug / stock):
```
Breeze    col  80px: diff=8   col 120px: diff=8   col 200px: diff=8
```

Fusion and Windows should still show diff=0 (unchanged by our patch).

---

## Test 2 — KeePassXC (hardest case)

KeePassXC entry properties was the original reporter of the bug — narrowest
column, most text, highest elision risk.

```
./run-clean.sh /usr/bin/keepassxc
```

> **Prereq**: KeePassXC requires a database to be unlocked before entry properties
> are visible. A test database was created during the 2026-07-15 Patch 1 verify run —
> confirm it loads before proceeding.
>
> **Theme**: Tools → View → Theme → Automatic (should be the default). Do NOT select
> a named theme — Automatic lets the session Breeze style drive the rendering, which
> is the path under test.

Check these tabs in entry Edit dialog:
- Entry tab (Title, Username, Password labels)
- Advanced tab (attribute key/value list)
- Icon tab (icon grid labels)
- Auto-Type tab (sequence list)
- Properties tab (key/value list — was the worst offender)

**Pass**: all labels fully readable, no "..." truncation.
**Fail**: any "..." visible where text should fit.

Screenshot: include terminal + app window.

---

## Test 3 — SMPlayer (Breeze style must be set manually)

SMPlayer defaults to Fusion. Must set Breeze manually inside the app.

```
./run-clean.sh /usr/bin/smplayer
```

In SMPlayer: Options → Preferences → General → Style → select "Breeze" → Apply.

Check Preferences sidebar labels:
- General, Interface, Subtitles, Audio, Video, Performance, Drives,
  Screenshots, Network, Keyboard and mouse, Advanced

"Keyboard and mouse" is the longest label — check it last.

**Pass**: all sidebar labels fully readable.
**Fail**: any label shows "..." at normal sidebar width.

Screenshot: include terminal + Preferences window with sidebar visible.

---

## Test 4 — GoldenDict (inherits session Breeze)

GoldenDict uses the session theme automatically (no manual style selection needed).

```
./run-clean.sh /usr/bin/goldendict
```

Check: View → Theme → Automatic (confirm it is set).
Check word list pane — multi-word entries like "Crab-eating macaque",
"Crab-lipped spider orchid".

**Pass**: full entries visible without truncation.
**Fail**: entries show "..." at normal pane width.

Screenshot: include terminal + main window with word list visible.

---

## Results

| Test                          | .1 (stock, 8px) | .2 (Patch 1, 0px) | .3 (Patch 2, 4px) | Notes |
|-------------------------------|-----------------|-------------------|-------------------|-------|
| Qt5 probe diff                | 8               | 0                 | 4                 | Exactly as predicted |
| KeePassXC sidebar             | FAIL            | PASS              | FAIL              | 4px still pushes over edge |
| KeePassXC Classic theme       | —               | —                 | FAIL              | Routes through breeze5.so; same result |
| SMPlayer sidebar (Breeze)     | FAIL            | PASS (mostly)     | FAIL              | "Keyboard and mouse" clips in all builds — physical column limit |
| SMPlayer sidebar (Default)    | —               | —                 | FAIL              | Default = Breeze on KDE session |
| SMPlayer sidebar (Fusion)     | —               | —                 | PASS              | Fusion diff=0 unaffected by our patch — confirms bug is in Breeze |
| SMPlayer codec list           | FAIL            | PASS              | FAIL              | Horizontal scrollbar also appears with bug/Patch 2 (CT_ItemViewItem inflation) |
| GoldenDict word list          | FAIL            | PASS              | FAIL              | |

---

## Decision: PATCH 1 LEADS THE MR

4px remaining discard (Patch 2) is enough to cause elision in all three apps
at their default column widths. Patch 1 (remove all horizontal narrowing, diff=0)
is the only fix that consistently clears all tested cases.

**Patch 2 is presented as "considered but insufficient"** in the MR description,
with the note: "4px remaining discard still causes visible elision in KeePassXC
entry properties panel and SMPlayer Preferences sidebar at default column widths."

**Bonus finding**: SMPlayer Fusion screenshot (same binary, same .3 install) shows
zero elision — confirms the defect is in Breeze's SE_ItemViewItemText, not the
application. Strong isolated comparison for the KDE bug report.

**SMPlayer "Keyboard and mouse"** clips in all builds including Patch 1. This is
the physical column width limit, not Breeze pixel discard — correct behavior.
