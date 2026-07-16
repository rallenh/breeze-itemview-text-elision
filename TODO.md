# TODO

## Patch work

- [x] Patch 1: Remove all horizontal text rect narrowing (built, installed, verified 2026-07-15)
- [x] Patch 2: Keep itemViewItemMargins, remove ItemView_ItemPaddingWidth (written, tested 2026-07-15)
- [x] Build Patch 2 via mock (plasma-breeze 6.7.3-1.fc43.3)
- [x] Install Patch 2 RPMs, run Qt5 probe — confirmed diff=4
- [x] Visual verify Patch 2: KeePassXC, SMPlayer, GoldenDict — all FAIL at diff=4
- [x] Decision: Patch 1 leads the MR; Patch 2 presented as "considered, insufficient"

## GitHub repo

- [x] Directory structure created
- [x] patches/0001 and 0002 written
- [x] probe/ programs copied + Makefile working
- [x] .gitignore (probe binaries committed intentionally — tiny, saves readers from needing Qt dev headers)
- [x] docs/bug-analysis.md
- [x] docs/style-plugin-loading.md
- [x] docs/keepassxc-qt6-analysis.md
- [x] README.md
- [x] screenshots/ populated: 01-stock-*, 02-patch1-*, 03-patch2-* (3 apps × 3 builds)
- [x] Screenshots reviewed for public audience (no sensitive data, no personal info)
- [ ] Allen: git init, add remote, push to github.com

## KDE

- [x] Draft bugs.kde.org ticket → kde-bugreport-draft.md
- [x] Verify Allen has a KDE Identity account (identity.kde.org) — needed for bugs.kde.org + invent.kde.org
- [x] Allen files bugs.kde.org ticket, notes the bug ID
- [ ] Draft invent.kde.org MR against plasma/breeze Plasma/6.7 branch (Patch 1)
- [ ] Allen opens MR, references bug ID in description
- [ ] Cross-link: update bug ticket with MR URL

## Fedora follow-up

- [ ] Once KDE ticket + MR are live, reply to any open Fedora packager threads with links
- [ ] Push Patch 1 build (6.7.3-1.fc43.2) to COPR after KDE MR is open
