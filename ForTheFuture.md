From historical and recent discussions within the official mailing lists (wmaker-dev, wmaker-user), the GitHub repository (window-maker/wmaker), IRC channels on Libera.chat, and the Reddit community (r/windowmaker), the most prominent requests and "wishlist" items from users and experts clearly emerge.  
Window Maker is a solid, lightweight window manager faithful to the NeXTSTEP aesthetics. However, the evolution of the modern desktop ecosystem (high-density displays, modern graphics pipelines, Wayland protocols) has generated a recurring list of requested features and improvements.

### **1. Native Wayland Support (or a Wayland-native successor)**

This is by far the most discussed topic across the communities.

* **The problem:** Window Maker is deeply tied to X11/Xlib primitives. With distributions progressively phasing out native X11 sessions, enthusiasts fear the desktop environment's obsolescence.  
* **The request:** For years, the community has been asking for a transition toward Wayland or an autonomous compositor based on wlroots. Given the sheer amount of work required to rewrite the core, this demand led to the creation of the sibling project wlmaker **(Wayland Maker)**—an independent Wayland compositor attempting to replicate Window Maker's look, Dock, and logic from scratch in a modern environment.

### **2. Native HiDPI and Fractional Scaling Support**

Icons and tiles for the Dock and DockApps have historically been fixed to square matrices (typically 64×64 pixels).

* **The problem:** On modern 2K/4K/Retina displays, the interface, window borders, context menus, and icons appear tiny or unreadable.  
* **The request:** Users request vector or scalable resizing for window decorations, system fonts, and Dock/Clip tiles, as well as the ability to handle multi-monitor setups with mixed pixel densities (DPI) without relying on complex hacks using `xrandr --scale`.

### **3. Advanced Multi-Monitor Management (Dynamic XRandR)**

Although Window Maker supports Xinerama and XRandR, modern external display management suffers from several known limitations:

* **What is missing:** Instant hot-plug recognition when connecting or disconnecting an external display (especially on laptops), persistence of icon and window positions assigned to specific screens, and the ability to duplicate or configure the Dock and Clip independently across monitors.

### **4. Integrated Compositing (True Transparencies and Shadows without Picom/Compton)**

Window Maker is not a compositing window manager.

* **The request:** To achieve soft drop shadows, true transparency, and smooth Alpha channel support (without video tearing), users must run external daemons like picom (or the older compton). A frequently requested feature is the integration of a minimal native compositing engine inside the WM, or cleaner support for XComposite/XDamage standards to eliminate graphical artifacts around menus and shaped icons.

### **5. System Tray / StatusNotifierItem Integrated into the Dock**

* **The problem:** Many modern applications (chat clients like Telegram/Discord, audio players, cloud sync tools) do not use traditional Window Maker dockapps, but rather the freedesktop notification area (StatusNotifier / System Tray).  
* **The request:** Currently, users must rely on third-party, often outdated solutions like wmsystemtray, wmsystray, or stalonetray embedded into the dock. A common request is a maintained, native dockapp or a dedicated Dock module acting as a modern freedesktop System Tray.

### **6. Assisted Tiling and Modern Window Snapping**

Despite being a pure stacking window manager, Window Maker includes rudimentary auto-tiling and smart maximization features (e.g., *Maximus*, horizontal/vertical split).

* **The request:** Influenced by modern tiling window managers (i3, bspwm) or modern desktop shortcuts, many users request intuitive shortcuts for grid snapping (half-screen, 2×2 quadrants, 3 columns on ultrawide monitors) by dragging windows to screen edges or via quick Super/Mod4 keybindings, without needing external helper tools.

### **7. Modernization of the DockApp Ecosystem and PipeWire Support**

Legacy DockApps (collected on *dockapps.net*) are often based on archaic audio libraries (OSS, raw ALSA) or battery status monitors incompatible with modern `/sys` filesystems.

* **The trend:** In recent discussions on Reddit and GitHub, a very practical and popular request concerns releasing and maintaining native dockapps for **PipeWire/PulseAudio** (such as recent *wmpulsemixer* forks), laptop screen brightness control applets, and NetworkManager-compatible network indicators.

### **8. WPrefs Updates and Full XDG Compliance**

* **WPrefs:** The proprietary graphical configuration tool is appreciated for its visual consistency, but is showing its age: it lacks intuitive interfaces for configuring newer EWMH shortcuts introduced in recent releases, lacks full UTF-8 support in certain tabs, and needs modern theme management.  
* **XDG Standards:** A classic request on the mailing lists is completing the migration to standard `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME` directories, reducing the hardcoded dependency on the legacy `~/GNUstep/Defaults/` directory.

---

## Feasibility Assessment

*Assessment added 2026-11-06, grounded in the measurements already collected in `BreakingToTheFuture.md` and `SpecAI.md` (the WM's own code is ~20.8% of instructions; the hot loops are Pango/HarfBuzz shaping and Xlib round-trips; SIMD and 4-byte-pixel rewrites were measured and rejected). Each item is rated for feasibility, likelihood of success, the concrete steps, and the user input required before starting.*

| # | Request | Feasibility | Likelihood | Effort / Risk |
|---|---|---|---|---|
| 1 | Native Wayland | Low (port) / High (wlmaker) | Low (port) | Separate project |
| 2 | HiDPI / fractional scaling | Medium | Medium (2×), Low (fractional) | Medium |
| 3 | Dynamic multi-monitor | Medium-High | High | Medium, incremental |
| 4 | Integrated compositing | Medium | Medium | High, standalone project |
| 5 | System tray / SNI | High | High | Low, isolated module |
| 6 | Tiling / snapping | High | High | Low, incremental |
| 7 | DockApp modernization / PipeWire | High (out of tree) | High | Low per app, ongoing |
| 8 | WPrefs / XDG | High | High | Medium, incremental |

### 1. Native Wayland Support

- **Feasibility:** Low as a *port*. The core is deeply X11/Xlib (`wWindowFor`, event dispatch, `wcore.c`, `wrlib` all assume `Display *`); a direct rewrite is the whole WM. High as a *separate* project — which already exists: **wlmaker** (Wayland Maker) is the community's answer.
- **Likelihood of success:** Low for a port inside this tree; High for wlmaker, which is the correct vehicle.
- **Steps:** none in wmaker. The pragmatic path is (a) keep wmaker as the X11/XWayland WM, (b) document XWayland usage for Wayland sessions, (c) direct Wayland demand to wlmaker.
- **User input required:** strategic decision — invest in wlmaker (not this repo), or declare wmaker X11-only and stop entertaining the port.

### 2. Native HiDPI and Fractional Scaling

- **Feasibility:** Medium. `wrlib` already scales (nearest `RScaleImage`, smooth `RSmoothScaleImage`); the Dock/Clip tiles and decorations are the hardcoded part (`ICON_SIZE`, fixed 64×64 matrices). Integer 2× is realistic; true fractional scaling (1.25×/1.5×) needs XRender compositing or per-monitor Xft DPI, which is much harder.
- **Likelihood of success:** Medium for 2×; Low for fractional without a compositor.
- **Steps:** (a) make Dock/Clip tile and decoration sizes runtime-scalable instead of fixed 64×64, (b) scale fonts/decoration metrics from the Xft DPI, (c) mixed-DPI multi-monitor needs per-output DPI (Xrandr) plus per-monitor icon caches.
- **User input required:** target priority (integer 2× first, or fractional?); real 2K/4K hardware to verify.

### 3. Advanced Multi-Monitor Management (Dynamic XRandR)

- **Feasibility:** Medium-High. `xinerama.c`/`randr.c` already handle Xrandr; what is missing is hot-plug reaction, per-screen persistence, and per-monitor Dock/Clip.
- **Likelihood of success:** High (incremental, low architectural risk).
- **Steps:** (a) verify/listen to `RRScreenChangeNotify` for hot-plug, (b) persist icon/window positions per output in WMState, (c) allow a Dock/Clip per monitor (or duplicate-on-all).
- **User input required:** laptop + external display to test hot-plug; desired behaviour (Dock on every monitor? Clip per monitor?).

### 4. Integrated Compositing (True Transparency/Shadows)

- **Feasibility:** Medium. Needs `XComposite` redirection, `XDamage` tracking and XRender (or GL) compositing. This is the largest standalone item and overlaps `BreakingToTheFuture.md` step 6, which is already flagged "highest risk, least evidence".
- **Likelihood of success:** Medium; a minimal XComposite+XRender compositor is achievable but is a project in its own right, and on Xvfb/llvmpipe it is neutral or slower.
- **Steps:** (a) `XCompositeRedirectSubwindows`, (b) `XDamage` tracking, (c) XRender blend of window pixmaps, (d) alpha/shadow on menus and shaped icons.
- **User input required:** integrate vs keep picom; scope (shadows/alpha only, or effects?). Decide after measuring on a real GPU.

### 5. System Tray / StatusNotifierItem

- **Feasibility:** High. A native dockapp (or a Dock module) speaking DBus/StatusNotifierItem is an *isolated* component — it does not touch the core.
- **Likelihood of success:** High (low risk, well-bounded).
- **Steps:** register on DBus, listen to `StatusNotifierWatcher`, render the icons as a Dock/Clip tile.
- **User input required:** shape — a standalone dockapp (dockapps.net style) or an integrated Dock module?

### 6. Assisted Tiling and Window Snapping

- **Feasibility:** High. Maximus and horizontal/vertical split already exist; grid snapping is an incremental addition to `moveres.c` (drag-to-edge detection) plus keybindings.
- **Likelihood of success:** High.
- **Steps:** (a) define snap zones (half, quadrants, 3-column), (b) Super/Mod4 keybindings, (c) drag-to-edge detection in the move/resize loop.
- **User input required:** which shortcuts and zones; whether snapping should be opt-in per preference.

### 7. DockApp Ecosystem Modernization and PipeWire

- **Feasibility:** High, but it is **out of tree** — these are external dockapps on dockapps.net, not wmaker code. wmaker needs no change.
- **Likelihood of success:** High per app; ongoing maintenance burden.
- **Steps:** fork/release specific dockapps (PipeWire/PulseAudio volume, brightness, NetworkManager) with modern backends.
- **User input required:** which dockapps to prioritize; whether Andrea wants to curate an official set.

### 8. WPrefs Updates and Full XDG Compliance

- **Feasibility:** High. WPrefs is a separate `WPrefs.app`; XDG is a path migration with a compatibility fallback.
- **Likelihood of success:** High.
- **Steps:** (a) WPrefs: UI for new EWMH shortcuts, UTF-8, theme management; (b) migrate defaults/state to `$XDG_CONFIG_HOME/wmaker` with a `~/GNUstep` fallback.
- **User input required:** priority (WPrefs UI vs XDG first); migration strategy (hard break vs read-both fallback).
