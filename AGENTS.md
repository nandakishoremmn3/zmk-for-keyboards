# Agent notes for this repo

Context for AI agents working on `nandakishoremmn3/zmk-for-keyboards` (branch `new-layout`).

## Two keyboards, one logical layout

- **Corne** (split, 42-key) — runs ZMK. Source of truth: `config/corne.keymap`.
- **Redragon K616 Fizz Pro** (60% ANSI) — runs kanata on Windows, translating physical
  QWERTY key positions to the same logical layout as the Corne. Source: `kanata/redragon_k616.kbd`.

**Sync policy (must stay in sync):** Treat these two files as a single logical layout split across two physical keyboards. Any combo, home-row mod, or layer-hold change in one file must be evaluated for porting to the other. If a concept cannot be mirrored 1:1 (matrix size, key count), document the divergence and the reasoning in this file and in inline comments. Never let the two drift silently.

### Combo & layout decisions (2026-09-02) — reasoning

**Clipboard family (bottom-row cluster, low collision bigrams):**
- `Shift+z` → Undo (`C-z`): kanata `(lsft z)` / ZMK `G+X (25 26)`. Chose `Shift+z` over bottom-cluster undo because `Shift` is already held for capital letters and `z` is at the edge of the cluster — rare natural bigram. Previous `G+X` on ZMK is kept as the Corne has no dedicated `Shift` key; it is the closest physical equivalent.
- `z+x` → Copy (`C-c`), `x+c` → Paste (`C-v`), `z+c` → Cut (`C-x`): 2-key adjacent/skipping bigrams on bottom row (`pos 26/27/28` on Corne = `X/J/K`; kanata `z/x/c`). Kept sequential and mnemonic.
- `c+v` → Win+V clipboard history (`M-v` / `LG(V)`): moved off `z+x+c` 3-key to a 2-key `c+v` per user request — 2-key is faster than 3-key and `c+v` is a rare English bigram. Previous `c+v` was Redo (`C-S-z`) — redo was dropped/moved.
- `z+x+c` → Redo (`C-S-z`): 3-key reserved for redo after `c+v` took over Win+V. ZMK keeps redo removed for now (matrix constraint: `26 27 28` already used for Win+V 3-key) — see Differences below.

**Delete/word-delete (top row):**
- `i+o` → `C-BSPC` (word-delete backward), `o+p` → `C-DEL` (word-delete forward). Changed from plain `BSPC`/`DEL` to word-level so the combo is actually useful for editing vs. tapping the dedicated keys. Both require `150 ms` idle + `30 ms` overlap to avoid misfire on fast `io`/`op` bigrams.

**Tmux/herdr prefix:**
- `m+,` → `C-b` (`pos 31 32` on Corne). Isolated on far right bottom row — almost no word contains `m,` adjacently, so misfire risk is negligible. Mirrored exactly.

**Home-row mods — GUI removal (2026-09-02):**
- **Problem:** `GUI` (Win/Cmd) on home-row `E` (pos 15) / `T` (pos 20) in ZMK and `e`/`t` in kanata (`mte`/`mtt`) caused **too many accidental triggerings** — every fast `e`→`a` or `t`→`a` roll fired GUI, even with `require-prior-idle-ms 150` / `chords-v2-min-idle 150`. The mod-vs-mod sequence still resolved as hold, spamming Win/Cmd and breaking typing. User requested removal.
- **Decision:** Remove GUI from home row entirely. Home row now: Corne `C=LSFT, I=LCTL, E=plain, A=LALT | H=RALT, T=plain, S=RCTL, N=RSFT` (kanata `e`/`t` plain, `bspc @mtc @mti e @mta , . @mth t @mts @mtn`). This keeps `Shift/Ctrl/Alt` on the home row where misfire tolerance is higher, but drops the most disruptive mod.
- **New location:** GUI is now a **pure** key on `B`:
  - kanata physical `b` (`R3` botrow, 6th key): `b` → `lmet` (pure `GUI`, no tap `b`). Chosen because `b` is bottom-row edge, rarely held, and is the logical equivalent of Corne's bottom-right left-half key.
  - Corne pos `29` (bottom-right of left half, previously `&to 1` / `X+J+K` cluster edge): `&to 1` → `&kp LGUI` (pure). The `B` logical key remains at top row pos `1` (`&kp B`) — no tap duplication. If a hold-tap `B`/`GUI` is desired later, change pos `29` to `&mt LGUI B` and kanata `b` to `mtb (b/lmet)`.

### Kanata vs ZMK — intentional differences (must document)

| Area | Kanata (`redragon_k616.kbd`) | ZMK (`corne.keymap`) | Reason |
|------|-------------------------------|----------------------|--------|
| **Caps Word** | `g+h (30 ms, all-released)` | `f+j (pos 16 19)` | `F`/`J` are home-row mods on kanata → fast rolls `f→j`/`t→a` misfired; moved to `G+H`. ZMK keeps `F+J` (index fingers, no mod conflict on that firmware). |
| **Win+V** | 2-key `c+v → M-v` (Gui+V) | 3-key `26 27 28 → LG(V)` | Corne matrix has no free 2-key adjacent pair for `c+v` without colliding with `paste (27 28)`; 3-key `X+J+K` is the only spare chord. Kanata prefers 2-key for speed. |
| **Redo** | `z+x+c → C-S-z` | Removed (`was 28 29 K+to1`) | Kanata can afford 3-key redo; ZMK dropped it to avoid stealing `Win+V` 3-key. Re-add at `28 29` if redo is needed on Corne. |
| **GUI** | `lmet` on physical `b` | `LGUI` on pos `29` (was `to 1`) | Same logical key, moved off home row to stop accidental triggers; different matrix positions. Both pure, no tap. |
| **MacOS layer** | Dropped entirely | `BASE_MACOS` (layer 1) with `LG(C/V/X/Z)` | Kanata is Windows-only; ZMK keeps Mac swaps for Bluetooth hosts. |
| **Thumb/layers** | `lctl lmet @navk @numk @symk rctl` — `ralt` is `BSPC` hold `SYM` | `LT2 ESC / MO4 / SPACE / ENTER / MO5 / BSPC` — thumb holds for `NUM/SYM/MOUSE/FUN` | Corne has 6 thumbs for layers; kanata has 6 mod-row keys and reuses `spc`/`bspc`/`esc` holds. |
| **Mouse** | `movemouse-accel-* (16 700 1 12)` + `mwheel-*` on NAV | `msc SCRL_*` + `mmv MOVE_*` + `mkp MB*` | Different stacks (kanata movemouse vs ZMK pointing). Tuning kept low initial + linear accel to avoid drift. |
| **Timing** | `chords-v2-min-idle 150`, `30 ms` per chord, `first-release` (most) | `require-prior-idle-ms 150`, `timeout-ms 30`, `first-release` | Intentionally matched so typing feel is identical. |
| **Deployed path** | `C:\ProgramData\kanata\kanata.kbd` (not AppData) | Desktop `corne_left.uf2`/`corne_right.uf2` via `build.sh` + Docker `zmk-build-arm:stable` | Different delivery (live reload vs DFU flash). |

Keep combos in sync between the two. When a combo is added to one, check whether it needs
backporting to the other.

### Combo backport status (kanata → ZMK)

- I+O → Backspace (word) — done (`combo_backspace`, key-positions `<8 9>` → `LC(BSPC)`)
- O+P → Forward Delete (word) — done (`combo_delete`, key-positions `<9 10>` → `LC(DEL)`)
- M+, → Ctrl+B (tmux prefix) — done (`combo_tmux_prefix`, key-positions `<31 32>`)
- `c+v` → Win+V, `z+x+c` → Redo, `z+x`/`x+c` clipboard — done (kanata 2-key/3-key, ZMK 3-key `paste_list_win`; redo currently kanata-only)
- G+H → Caps Word — intentionally NOT mirrored; kanata moved it off F+J because F/J are
  home-row-mod keys there and collided with fast typing. ZMK keeps caps-word on F+J
  (`combo_caps_word`).
- GUI removal `E`/`T` → plain, GUI → `B`/`pos29` — done (both files, 2026-09-02)

## Kanata on Windows (autostart)

- Autostarts via a Startup-folder shortcut:
  `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\Kanata.lnk`, targeting the
  `kanata_windows_gui_winIOv2_x64.exe` (tray) build, with the shortcut's native
  "Run as administrator" flag set. Elevation is required so kanata can send input to
  elevated windows (Task Manager, File Explorer) — Windows UIPI otherwise blocks it.
- **Live/deployed config lives at `C:\ProgramData\kanata\kanata.kbd` /
  `C:\ProgramData\kanata\noop.kbd`** — NOT `C:\Users\<user>\AppData\Local\kanata\`.
  That AppData location became invisible/inaccessible to real UAC-elevated processes for
  reasons never conclusively root-caused (ACLs, AV, and Controlled Folder Access were all
  ruled out); moving the deployed config to `C:\ProgramData\kanata` sidestepped it.
  After editing `kanata/redragon_k616.kbd` in the repo, copy it to
  `C:\ProgramData\kanata\kanata.kbd` to deploy.
- Task Scheduler was tried as an autostart mechanism and found unreliable in this
  environment (registers fine, fails to launch — `ERROR_DIRECTORY`). Don't use it; stick
  with the Startup-folder shortcut.

## ZMK keymap visualization

- `keymap-drawer` renders `config/corne.keymap` to `corne_keymap.svg`:
  ```
  keymap parse -z config/corne.keymap > /tmp/corne_keymap.yaml
  keymap draw /tmp/corne_keymap.yaml > corne_keymap.svg
  ```
- Working version pins in this WSL environment: `tree-sitter==0.24.0`,
  `tree-sitter-devicetree==0.14.0`, `keymap-drawer==0.21.0`. Newer `tree-sitter` breaks
  keymap-drawer's `.query()` call; newer `tree-sitter-devicetree` needs a newer
  `tree-sitter` ABI than 0.24.0 supports.
- `keymap-viewer.html` / `keymap-viewer-typewriter.html` are hand-crafted static pages
  with hardcoded JS data — they are NOT auto-generated and go stale after any
  `config/corne.keymap` edit. Update their embedded JS manually if an up-to-date
  interactive viewer is needed.

## Firmware build

- `build.sh` builds both Corne halves via Docker (`zmkfirmware/zmk-build-arm:stable`),
  producing `corne_left.uf2` / `corne_right.uf2`, exported to the Windows Desktop.
- nice!nano (nRF52840) usable FLASH is ~792 KB (of 1 MB total; rest reserved for
  SoftDevice + MCUboot), RAM 256 KB. Exceeding either fails at link time with an explicit
  error — no silent overflow risk.

## Environment quirks

- The Bash tool's default shell has no direct WSL filesystem access. To run Linux
  tooling (pip, `keymap`, docker, west), invoke via `wsl.exe -e bash -lc "<command>"`.
- Use the `\\wsl.localhost\ubuntu\...` UNC path for Read/Write/Edit/PowerShell tools
  against repo files.
- `Glob`/`Grep` over the repo root can time out due to the large untracked `zmk/` nested
  checkout (~4.6 GB) — scope searches to specific subdirectories where possible.
