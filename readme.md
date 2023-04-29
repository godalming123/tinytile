# Tinytile
A fork of tinywl designed to be a simple base for a tiling WM (but currently we have no features for tiling)

# Building from source
1. install dependencys (also install with `-devel` if you distro provides those packages):
	- wlroots
	- wayland-protocols
	- meson
	- pango
	- cairo
	- alacritty (recommended but not techinically necersarry)

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
	- It is recomended that you change your keyboard layout from the default which is gb to your keyboards layout. This can be changed by editing `config.h` and ounce you have started tinytile, it will only change again if you:
		- modify `config.h`
		- rerun `ninja -C build` and `ninja -C build install`
		- relaunch tinytile.
3. run `ninja -C build`
4. run `ninja -C build install`
5. run tinytile with the command `tinytile`

# Usage
- To see usage instructions launch the compositor and then press alt + h

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
 - Their is a momory leak when opening messages such as the hello message
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
 - Add maximised windows (the current implementation is also naive)
 - Use alt + mouse to drag/resize windows
 - When a window is closed the next window does not get focused
 - When dragging popup windows it should drag the parent instead
 - Use libinput to configure pointers
 - Switching windows in reverse focus order gives wierd behavior
 - Add option to make windows both have or don't have CSD and wether windows should be set to tiled
 - when you reorder windows and then release alt before shift the dialogue stays on screen (fixed by using mod to reorder windows instead of alt + shift)
## Implement protocols
 - Virtual keyboard (needs testing)
 - Drag icon
