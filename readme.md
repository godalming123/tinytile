# Tinytile
A fork of tinywl designed to be a simple base for a tiling WM

# Building from source
1. install dependencys (also install with `-devel` if you distro provides those packages):
	- wlroots
	- wayland-protocols
   EG for arch:
   ```
pacman -S wlroots wayland-protocols
   ```
2. run `meson build`
3. run `ninja -C build`
4. run `ninja -C build install`

# Todo
## Implement protocols
 - Layer shell
 - Idle_inhibitor_v1
 - Drag icon
## Implement config options
 - sloppy focus
 - Tiling
 - Changing monitor configurations
 - Use alt + mouse to drag/resize windows
## Fix bugs
 - gnome web cannot display websites (note: when installed as system package instead of flatpak it works (however the system package is gtk3 based which may be the reason for this))
 - When a window is closed the next window does not get focused
# Done
 - Add a way to close windows with the keyboard
 - Allow switching virtual terminals
 - Add configuration for pointers via libinput
 - Changing keymaps
 - Keybindings on release
 - Add fullscreen windows (the current implementation is very naive and untested on multiple monitor setups)
## Implement protocols
 - Virtual keyboard (needs testing)
