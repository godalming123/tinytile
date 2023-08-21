# TINYTILE
A fork of tinywl designed to be a simple base for a tiling WM with less then 2000 lines of code under the BSD-3-Clause license (see LICENSE for more details).
***NOTE:*** that this is not the default branch, but instead one that is re-written with simalar goals, the similaritys END THERE.

# Stargazers over time
[![Stargazers over time](https://starchart.cc/godalming123/tinytile.svg)](https://starchart.cc/godalming123/tinytile)

# Configuration
Configure tinytile with options when you run the launch command, you also need to note that for space-separeted inputs use an underscore instead of a space EG:
```shell
tinytile\
  keyboradLayout gb\
  keyboardOptns  caps:escape\
  terminal       alacritty\
  browser        qutebrowser\
  systemMonitor  alacritty_-e_btop\
  hideCursor     yes
```

# Keybindings
Use alt + `x` to `y` where:
| `x` is ... | `y` is ...                 |
|------------|----------------------------|
| q          | close the focused window   |
| j          | focus the next open window |
| return     | open a terminal            |
| b          | open a web browser         |
| m          | open a system monitor      |
| x          | suspend the system to ram  |
| p          | power off the system       |
| r          | reboot the system          |

# Style guide
 - Format every `.c` and `.h` file with the provided `.clang-format` file
 - Use `/* COMMENT */` for comments and `//` to comment out code blocks
 - Name every variable with `snake_case`, enum values must `USE ALL CAPS` as well
 - Here are some rules that may be broken, which if I can be bothered will make me rebase from default tinywl:
   - Commits *should* be atomic - each commit implements one thing.
   - The implementation of these features *should* only be modified if they need to work togethor with larger features - allows new developers to see how to implement everything individually ontop of base tinywl

# Todo
## The basics:
 - [ ] Cursor pressed move off surface fix
 - [ ] Make gtk popups spawn in the center of their parents
 - [ ] Make sure that the mouse always has the correct icon for what it is pointing over:
    - When you open a window the cursor can be the wrong icon
 - [ ] Implement drag icons
 - [ ] Implement screen recording and screenshotting:
    - [ ] Implement the output manager protocol
    - [ ] Implement the scrrencopy protocol
 - [WIP] Stop `xdg_popup`s from going off screen (some surfaces still go offscreen)
 - [X] Add an option to hide the cursor if it is at the top left of the screen which allows for less distractions
 - [X] Allow switching virtual terminals
 - [X] Add configuration for pointers via libinput (allows for mouse acceleration)
 - [X] Make clients think they are tiling so that they draw square borders
 - [X] Basic keybindings for:
    - [X] Suspending/shutting down/rebooting
    - [X] Opening a terminal
    - [X] Opening a web browser
    - [X] Opening a system monitor
    - [X] Closing the focused window
 - [X] Basic config options for things such as - need to be customizable with command line arguments:
    - Keyboard layout
    - Keyboard options (to rebind keys)
    - Command to be ran to open a terminal
    - Command to be ran to open a web browser
    - Command to be ran to open a system monitor
## The bug fixes:
 - [ ] Clients start at a floating size and then resize to the new size which wastes CPU and is visible to the user
 - [ ] Command are executed in `sh` which is unnecarserry
## Things that will take more time:
 - [ ] A menu system (requieres font rendering):
    - [ ] Be able to switch (and reorder) apps with a list of open windows
    - [ ] An overlay panel in the corner of the screen to display info such as:
       - Date & time
       - Battery available and wether it is charging, discharging or staying the same
       - CPU tempreture
       - Memory usage as % and GB/MB used
       - Perhaps how many updates you have
       - Network status
 - [ ] Tiling functionality (and removal of floating functionality)
    - [ ] Splits
    - [ ] Be able to manage windows effectively with multiple monitors
    - [X] Fullscreen clients
    - [X] Spawn windows maximized to the pointer focused monitor by default
    - [X] Make sure that when you close a window it focuses another window
## Things that I consider bloat and will not implement
 - XWayland support
 - Layer shell support
