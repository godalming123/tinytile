# Tinytile

A fork of tinywl designed to be a simple base for a tiling WM (but currently we have no features for tiling).

Note: there is a feature demo [here](https://youtu.be/maESP00L36M).

## Stargazers over time

[![Stargazers over time](https://starchart.cc/godalming123/tinytile.svg)](https://starchart.cc/godalming123/tinytile)

# Installing  with a binary

Currently we have no support for binary packages or installing manually with a binary however this is planned in the future, and if you have an idea of how to achieve this then an issue/PR would be much appreciated.

# Building from source

## 1. Install dependencys
| Distro | Command to install packages                                                           |
|--------|---------------------------------------------------------------------------------------|
| Arch   | `sudo pacman -S wlroots wayland-protocols meson pango pkg-config`                     |
| Fedora | `sudo dnf install wlroots-devel wayland-protocols-devel meson pango-devel pkg-config` |
 - In addition to this you will need **a monospace font** and **a c compiler**.

If your distro of choice is not on this list then:
 - Check that the distro packages wlroots 0.16 if not then you will have to compile wlroots yourself
 - If your distro has wlroots 0.16 then you can create an issue and I will try to test your distro and see what packages you need to install
 - You could also try to create a list of needed packages yourself and if you are succsessful remember to tell me the package names and I will add the packages to our dependency table

## 2. Run `meson setup build`
This command configures a ninja buildfile in the `build/` directory to do the compiling. If you get an error that is likely because you do not have the right dependencys installed although feel free to create an issue if you do not think that this is the case.

## 3. Configure settings for you
Configuration can be both compile time (in `config.h`) or runtime (with the `example-start.sh`) script.
It should also be noted that some options are slightly more verbose in the `config.h` - they do not use perfectly the same names and syntax.

## 4. Run `ninja -C build`
This actually builds the software into an executable for your system.

## 5. *(optional)* Run `ninja -C build install`
This installs the executable into `/usr/bin` for you and required if you want to use runtime configuration.

## 6. Run tinytile with the command `tinytile` or `./build/tinytile`
This starts the compositor, the reason that their are 2 commands is that if you did not install it to `/usr/bin` then you must use the later command.

# Usage

## Keybindings:
Items marked with ⚙️ are configurable (see above section on configuration).
 - *General:*
    - Just alt                 - toggle a simple meny where if you type and press enter you can launch apps
    - Alt + hold left click    - move a client
    - Alt + hold right client  - resize a client
    - ALt + escape             - exit this compositor
    - Alt + h                  - open the project usage instructions in a browser
    - Alt + x                  - sleep your system
    - Alt + enter              - open your terminal ⚙️
 - *Keybindings for managing clients:*
    - Alt + q                  - close focused client
    - Alt + f                  - toggle wether a client is maxmized
    - Alt + c                  - center the client
    - Alt + w/s                - go to previous/next clients with a menu
    - Super + w/s              - swap the focused client with the previous/next client (also with a menu)
    - Alt + a                  - show a list of open clients (hides as soon as you release alt)
 - *Keybindings for moving/resizing clients:*
    - Alt + shift + W/A/S/D    - move a client
    - Alt + control + w/a/s/d  - resize a client

# Todo

## Feature additions
 - Sloppy focus
 - Tiling
 - Changing monitor configurations
 - Make the search actually *search* for apps rathor then just running a command
 - Add more info to the alt menu (this list is everything I want in the final menu):
    - Date & time
    - Help such as type to search and a keybinding to see the help page
    - CPU usage
    - Memory usage as percantage *and* GB/MB used
    - If you are on laptop, then battery charge and wether it is increasing, decreasing or staying the same
    - An indicator that is displayed if you are not connected to internet (to check this I could use ping)
    - Maybe show open apps in this menu rathor the the alt + a menu

## Things that you might need but are not implemnted nor todos
 - XWayland support
 - Layer shell support

## Bug fixes
 - Their is a momory leak when opening messages such as the hello message
 - When resizing a client with a popup/parent it should keep the popup centered to the parent
 - Sometimes subsurfaces can go offscreen
 - When you close and re-open a GTK window that was maximized it appears in the top-left thinking it is maximized but not being maximized
 - If you launch tinytile in another TTY and open an app that you had launched on another instance of tinytile it appears on the other instance (although this may be a wayland problem)
 - Another bug related to this is if you then switch to the TTY where the app was launched, close the app with a keyboard shortcut and open the window menu, then no window shows as focused and the compositor crashes when you close the window menu

# Done
 - Use meson instead of ninja
 - Update argument passing
 - Allow switching virtual terminals
 - Cursor pressed move off surface fix
 - Make closing one client focus the next
 - Add configuration for pointers via libinput
 - Add fullscreen clients
 - Add maximised clients
 - Make new clients launch on the center of the focused monitor
 - Make draging a popup drag the parent instead
 - Support changing keymaps in the config.h
 - Add font rendering using pango
 - Implement keybindings for basic desktop usage such as:
    - Closing clients
    - Switching clients with a list
    - Rearranging clients also with a list
    - Maximizing clients
    - Centering clients
    - Opening the browser to this projects help page
    - Moving/resizing clients with alt+mouse
    - Moving/resizing clients with the keyboard
    - Launching a terminal, browser, and file explorer
 - Add option to make clients both have or don't have CSD and wether clients should be set to tile
 - Make a simple menu to run commands
 - Allow configuration at run-time with options
## Implement protocols
 - Virtual keyboard (needs testing)
 - Drag icon
 - Screencopy (although for some screen recorders this does not work as we do not implement the `output_manager` protocol)
