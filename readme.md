# Tinytile
A fork of tinywl designed to be a simple base for a tiling WM (but currently we have no features for tiling)

# Building from source
1. install dependencys (also install with `-devel` if you distro provides those packages):
	- wlroots
	- wayland-protocols
	- meson
  - pango
  - cairo
  - alacritty (recomended but not techinically necersarry)

EG for arch:
```
sudo pacman -S wlroots wayland-protocols meson pango
```
Or for fedora:
```
sudo dnf install wlroots-devel wayland-protocols-devel meson gcc pango-devel
```
2. run `meson build`
3. configure keyboard layout
It is recomended that you change your keyboard layout from the default which is gb to your keyboards layout. This can be changed by editing `config.h` and ounce you have started tinytile, it will only change again if you:
  - modify `config.h`
  - rerun `ninja -C build` and `ninja -C build install`
  - relaunch tinytile.
3. run `ninja -C build`
4. run `ninja -C build install`
5. run tinytile with the command `tinytile`

# Usage
## Default keybindings
 - **Alt + escape** - exit this compositor
 - **Alt + q** - close focused window
 - **Alt + enter** - Open alacritty (a terminal emulator)
 - **Alt + x** - sleep your system using systemctl
 - **ALt + w/s** - go to next/previous window
 - **Alt + a** - show a list of open windows
 - **Alt + n** - open nautilus
 - **Alt + f** - open firefox
 - **Alt + h** - open a help menu when in the compositor
## Other behaviours
 - You can press alt and then tap and hold on a window with left click to drag it or right click to resize it

# Todo
## Implement protocols
 - Layer shell
 - Idle_inhibitor_v1
 - Lock surface
## Implement config options
 - Sloppy focus
 - Tiling
 - Changing monitor configurations
 - Make new windows launch on the focused monitor rathor then 0, 0
## Fix bugs
 - Gnome web cannot display websites - note: when installed as system package instead of flatpak it works (however the system package is gtk3 based which may be the reason for this)
 - Drag icon protocol randomly crashes the compositor when your mouse is not hovering a surface
 - When resizing a client with a popup/parent it should keep the popup centered to the parent
 - Sometimes subsurfaces can go offscreen
# Done
 - Add font rendering using pango
 - Add a way to close windows with the keyboard
 - Allow switching virtual terminals
 - Add configuration for pointers via libinput
 - Changing keymaps (currently we assume GB keymap)
 - Keybindings on release
 - Add fullscreen windows (the current implementation is very naive)
 - Use alt + mouse to drag/resize windows
 - When a window is closed the next window does not get focused
 - When dragging popup windows it should drag the parent instead
 - Use libinput to configure pointers
 - Switching windows in reverse focus order gives wierd behavior
## Implement protocols
 - Virtual keyboard (needs testing)
