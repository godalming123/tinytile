# TINYTILE
***NOTE:*** that this is not the default branch, but instead one that is re-written in zig with simalar goals, the similaritys END THERE.
A fork of tinywl designed to be a simple base for a tiling WM written in zig with wlroots version `0.16`.

# Stargazers over time
[![Stargazers over time](https://starchart.cc/godalming123/tinytile.svg)](https://starchart.cc/godalming123/tinytile)

# Other language options that could be considered for this project:
## Not usable becuase the bindings are still on 0.15:
 - Golang - has a PR for wlroots 0.17 which will be merged when that is released
 - Rust
## Not usable because the wlroots bindings are far out of date (3+ years):
 - Haskell
 - Ocaml
## Not usable because the languages cannot build wayland compositors:
 - Godot
 - Qt
## Not usable because they are irrelavent languages:
 - Chicken scheme
 - Common lisp
 - Swift
## The languages that are left:
 - C/C++ - both good choices however I do not see a need for any C++ features
 - Zig - good languages, unfortinute that it is still in development
 - Python - too slow + over complexity

# Todo
## The basics:
 - [ ] Allow switching virtual terminals
 - [ ] Cursor pressed move off surface fix
 - [ ] Add configuration for pointers via libinput (allows mouse acceleration)
 - [ ] Make gtk popups spawn in the center of their parents
 - [ ] Disable CSD and make clients think they are tiling
 - [ ] Make sure that the mouse always has the correct icon for what it is pointing over
 - [ ] Implement drag icons
 - [ ] Implement the screen copy protocol
 - [ ] Basic keybindings for:
    - [ ] Suspending/shutting down/rebooting
    - [ ] Opening a terminal
    - [ ] Opening a web browser
    - [X] Closing the focused window
 - [ ] Basic config options for things such as - need to be customizable with command line arguments:
    - Keyboard layout
    - Keyboard options (to rebind keys)
    - command to be ran to open a terminal
    - command to be ran to open a web browser
## The bug fixes:
 - [ ] Clients start at a floating size and then resize to the new size which wastes CPU and is visible to the user
## Things that will take more time:
 - [ ] Tiling functionality (and removal of floating functionality)
    - [ ] Fullscreen clients
    - [ ] Splits
    - [X] Spawn windows maximized to the pointer focused monitor by default
 - [ ] A menu system (requieres font rendering):
    - [ ] Be able to switch (and reorder) apps with a list of open windows
    - [ ] An overlay panel in the corner of the screen to display info such as:
       - Date & time
       - Battery availble and wether it is charging, discharging or staying the same
       - CPU tempreture
       - Memory usage as % and GB/MB used
       - Perhaps how many updates you have
       - Network status
## Things that I consider bloat and will not implement
 - XWayland support
 - Layer shell support
