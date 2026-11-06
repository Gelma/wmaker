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
