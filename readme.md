# Tinytile
A fork of tinywl designed to be a simple base for a tiling WM (but currently we have no features for tiling)

# Building from source
## 1. Install dependencys
| Distro | Command to install packages                                                    |
|--------|--------------------------------------------------------------------------------|
| Arch   | `sudo pacman -S wlroots wayland-protocols meson pango`                         |
| Fedora | `sudo dnf install wlroots-devel wayland-protocols-devel meson gcc pango-devel` |

If your distro of choice is not on this list:
 1. Check that the distro packages wlroots 0.16 if not then you will have to compile wlroots yourself
 2. If your distro has wlroots 0.16 then you can create an issue and I will try to test your distro and see what packages you need to install
## 2. Run `meson build`
This command configures a ninja buildfile in the `build/` directory to do the compiling.
If you get an error that is likely because you do not have the right dependencys installed.
## 3. Configure settings for you
Their are several settings that can be configured in the config.h file such as:
 - keyboard layout (default: `gb`)
 - command to run a terminal (default: `alacritty`)
 - command to run a browser (default: `qutebrowser`)
 - command to run a filemanager (default: `nautilus`)
## 4. Run `ninja -C build`
This actually builds the software into an executable for your system.
## 5. Run `ninja -C build install`
This installs the executable into `/usr/bin` for you.
## 6. Run tinytile with the command `tinytile`
This starts the compositor.

# Usage
 To see usage instructions launch the compositor and then press alt + h

# Todo
## Feature additions
 - Sloppy focus
 - Tiling
 - Changing monitor configurations
 - Make new windows launch on the focused monitor rathor then 0, 0
## Bug fixes
 - Their is a momory leak when opening messages such as the hello message
 - When resizing a client with a popup/parent it should keep the popup centered to the parent
 - Sometimes subsurfaces can go offscreen

# Done
 - Use meson instead of ninja
 - Update argument passing
 - Allow switching virtual terminals
 - Cursor pressed move off surface fix
 - Make closing one window focus the next
 - Add configuration for pointers via libinput
 - Add fullscreen windows (the current implementation is very naive)
 - Add maximised windows (the current implementation is also naive)
 - Make draging a popup drag the parent
 - Support changing keymaps in the config.h
 - Add font rendering using pango
 - Implement keybindings for basic desktop usage such as:
    - Switching windows with a window list
    - Rearranging windows also with a window list
    - Closing windows
    - Making windows take up the whole screen
    - Getting a help menu with a list of keybindings
    - Moving/resizing windows with alt+mouse
    - Launching a terminal, browser, and file explorer
 - Add option to make windows both have or don't have CSD and wether windows should be set to tiled
## Implement protocols
 - Virtual keyboard (needs testing)
 - Drag icon
