# Local fork notes (personal Windows build; committed only in the personal fork, never part of upstream PRs)

The user runs a locally built Vicinae on Windows with personal patches on top of upstream `main`.
When they ask to "pull the latest changes", "update", "merge" or "rebase", follow the workflow below so the patches
survive.

The fork on GitHub is public: keep usernames, absolute user paths and other personal details out of these files.
Machine-specific settings go in `build-local.env.bat` (gitignored, see `build-local.bat`).

## Remotes and branches

- `origin`: upstream `vicinaehq/vicinae`. `fork`: the user's public GitHub fork.
- `main` tracks `origin/main`. `patched` is `main` plus the patches below and the local build setup (these notes,
  `build-local.bat`, `mise.local.toml`). It's the branch that gets built and installed.
- Upstream is merged into `patched`, never rebased onto (the user's choice as of 2026-10-17: one conflict pass against
  the final code, and existing commits keep their behavior). Don't rewrite `patched` history or force-push it.
- New patches go on top of `patched` as their own commit. Up to 2026-10-17 the patches were kept as a linear stack,
  one commit each, with `chore: local Windows build setup` last; history after that includes merge commits.
- `git diff origin/main..patched` shows the full local delta (two dots). To send something upstream, build a clean
  branch from `origin/main` based on that diff, leaving out the build setup files. Patch 7 depends on patch 2.

## Patches

1. `fix(windows): put the default handler first in file openers`
   `WindowsAppDatabase::findOpeners` (`src/server/src/services/app-service/windows/win-app-database.cpp`) puts the
   `AssocQueryString` default handler first, then the `SHAssocEnumHandlers` ones, deduplicated by app id.
   Why: file search uses `findCuratedOpeners().front()` as the Enter action, and the enumeration order isn't the user's
   default (mkv opened in Windows Media Player instead of mpv).
2. `feat(file-search): add a persistent toggle for the detail panel`
   "Toggle Details" action (Ctrl+Shift+D) in `SearchFilesSection::actionPanel`; state in `SearchFilesViewHost`
   (`m_detailEnabled`, stored as `showDetail` in the command's local storage, like `fileCategory`).
   Why: the 35% list pane truncates long file names.
3. `fix(raycast-compat): honor Action.Open's application prop`
   `src/typescript/raycast-api-compat/src/components/action.tsx` wraps `Action.Open` to map Raycast's `application` /
   `onOpen` props (vicinae's own `Action.Open` only reads `app`); compat `open()` in `system.ts` resolves `id`,
   `bundleId` or `windowsAppId`. Why: git-repos' "Open in <app>" actions all opened Explorer.
4. `fix(qml): don't run deferred scroll calls on unloaded previews`
   `TextViewer.qml` and `ScrollViewport.qml` defer through a zero-interval `Timer` instead of `Qt.callLater`.
   Why: a Loader that unloads a preview clears its QML context but only `deleteLater()`s it; `Qt.callLater` still
   runs the queued call there and the AOT code crashes (Qt6Qml.dll access violation when holding Down in file search).
   Good upstream PR candidate. On conflicts, re-apply to any new `Qt.callLater` in Loader-hosted detail components.
5. `fix(windows): keep the UI thread off a stalled Everything query`
   The Everything window client sends its query with `SendMessageTimeoutW` (5s, `SMTO_ABORTIFHUNG`) instead of an
   unbounded `SendMessageW`, and `EverythingFileIndexer::isAvailable()` uses `try_lock`, serving the last known
   availability while a query is in flight. Slow queries, IPC failures and slow capability probes are logged.
   Why: a stalled Everything froze the launcher until Windows killed it as "not responding".
6. `feat(search): rank previous picks for the typed text first`
   Listary-style ranking: `VisitTracker` remembers up to 16 normalized queries per item; root search orders by alias,
   then query affinity, then fuzzy score. `FileService` keeps a query-keyed file history (`file-visits.json`) and
   re-ranks file results by it, re-adding previous picks the indexer didn't return. Everything results are ranked by
   fuzzy file-name match over a wider date-ordered window.
7. `feat(file-search): show the parent folder when the detail panel is off`
   `SearchFilesSection::setShowParentPath` makes the subtitle the compressed parent directory while the detail panel is
   hidden. `ListItemDelegate.qml` has an opt-in `subtitleIsPath` mode (set via `CommandListView.subtitleIsPath` in
   `SearchFilesView.qml`): the title keeps its full width, the path gets the rest (hidden below 60px) and elides in
   the middle. Why: same-named results couldn't be told apart without the panel.
8. `fix(windows): retry pending shell icons instead of caching the fallback`
   `renderWinShellIcon` retries `GetImage` on `E_PENDING` (5 × 100ms, like Listary's 5 × 200ms); `ImageStream`
   no longer stores a fallback under a failed `WinShellIcon`'s cache keys (`m_cacheFallbackAsOriginal`); app icons
   fall back to the stock application icon (`SIID_APPLICATION`) instead of the "?". Why: the shell returns
   `E_PENDING` for icons not yet in its cache, and the "?" then stuck to Chrome for the life of the process.
9. `fix(windows): recover modifier shortcuts from missed key ups`
   `windows-global-shortcut-backend.cpp`: outside capture, the hook drops tracked modifiers (`g_heldMods`) that
   `GetAsyncKeyState` says are up, and `dispatchModifier` starts a new chord when a modifier goes down alone.
   Why: Win+L locks on the L key down, so the Win/L key ups never reach the hook; the stale held Win and broken
   chord made the next bare Win press open the Start menu instead of Vicinae. Keep `g_heldMods` itself: the
   shortcut recorder eats modifiers, so the async key state never sees them there. Good upstream PR candidate.
10. `fix(windows): draw the launcher border on Windows 10`
   `WindowsWindowAttached::nativeChromeSupported()` (Windows 11+) gates the DWM border/rounding; below it
   `LauncherWindowWindows.qml` sets `drawBorder` so `LauncherWindow.qml` draws its QML border ring, and
   `nativeCornerRadius()` is 0 (Windows 10 windows are square). Why: `DWMWA_BORDER_COLOR` is Windows 11 only, so on
   Windows 10 the launcher had no border and the theme's `main_window.border` did nothing. Good upstream PR candidate.

Patches 1–4 had no matching upstream PR/issue as of 2026-10-07; 5–10 weren't checked. Once upstream ships a patch's
change, the merge usually absorbs it; if upstream solved it differently, `git revert` the patch and remove its entry
here.

## Update workflow

```
git fetch origin
git checkout patched
git merge origin/main             # resolve conflicts keeping the intent above
build-local.bat build             # run `build-local.bat configure` first if CMake files changed or the build dir is gone
build-local.bat installer         # -> build-relwithdebinfo\vicinae-x64-setup.exe
git push fork patched
```

Also fast-forward `main` to `origin/main` (`git branch -f main origin/main` while on `patched`).

Format changed C++ with clang-format 23.1.0 (CI's pinned version): `pip install --target <dir> clang-format==23.1.0`,
then run `<dir>\clang_format\data\bin\clang-format.exe` (the `<dir>\bin\clang-format.exe` wrapper fails without the
package on `sys.path`). Then quit Vicinae and run the installer: it upgrades the per-user install in
`%LOCALAPPDATA%\Programs\Vicinae\` in place. Official upstream installers overwrite these patches. A silent install
(`/VERYSILENT /SUPPRESSMSGBOXES /NORESTART`) closes the running Vicinae and doesn't restart it; start
`%LOCALAPPDATA%\Programs\Vicinae\bin\vicinae-server.exe` afterwards.

## Toolchain (all local except Build Tools)

- MSVC: Visual Studio 2022 Build Tools (matches CI `depot-windows-2022-16`). Chosen over 2026 deliberately.
- CMake, Ninja, uv, aqtinstall: mise, pinned in `mise.local.toml`.
- Qt 6.10.1 `win64_msvc2022_64` + qtshadertools, qtimageformats (same as CI), in `.toolchains\Qt` next to the repo
  checkout (`build-local.bat`'s default).
- Inno Setup 6 per-user (for `scripts/mkinstaller.ps1`).

`build-local.bat env` prints the detected `QTDIR`, `MISE` and `VSDIR`; override any of them in `build-local.env.bat`.

### Setting up a new machine

From the folder that contains the repo checkout:

```
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install jdx.mise
winget install JRSoftware.InnoSetup --scope user
mise exec pipx:aqtinstall@3.3.0 -- aqt install-qt windows desktop 6.10.1 win64_msvc2022_64 -m qtshadertools qtimageformats -O .toolchains\Qt
cd vicinae
mise install                              # tools from mise.local.toml; run `mise trust` if asked
build-local.bat configure
build-local.bat build
build-local.bat installer
```

Run aqt from outside the repo, otherwise it drops `aqtinstall.log` into it.

## Gotchas

- On a non-UTF-8 Windows codepage (e.g. CJK locales), upstream `CMakeLists.txt` adds `/utf-8` after `add_subdirectory`
  of the vendored libs, so `src/lib/glyph` fails with C2001 unless `/utf-8` is in `CMAKE_CXX_FLAGS` (done by
  `build-local.bat configure`). Good small upstream PR: move the MSVC `add_compile_options` block above the
  subdirectories.
- The build runs Qt-linked tools (`katehighlightingindexer.exe`); Qt `bin` must be on PATH or they fail with 0xc0000135
  (`build-local.bat` adds it).
- Agent shells set `NoDefaultCurrentDirectoryInExePath=1`, so a bare `build-local.bat` isn't found: run
  `cmd /c "<repo>\build-local.bat build"`. In those shells `vcvars64.bat` also prints a harmless
  "'vswhere.exe' is not recognized".
- Self-update is off for this build: `WindowsUpdateInstaller` requires a signed binary (CI signs releases; local builds
  aren't), so `checksSupported()` is false. No update checks, no notifications (log: "Running binary is not signed,
  self update is disabled"). Upstream releases never overwrite the patches automatically, but the user also isn't told
  about them. Updating means merge and rebuild.
- Before running the installer, stop any dev-build server started from `build-relwithdebinfo\bin`: the installer's
  `CloseApplications=force` only closes processes running from the install folder.
- To run a dev build without installing: put Qt `bin` on PATH and start `build-relwithdebinfo\bin\vicinae-server.exe`.
