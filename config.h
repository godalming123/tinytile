// This is the most important thing to change
const char keyboardLayout[] = "gb";

// A monospace font is recomended because several menus rely on charecters being the same width
const char font_description[] = "Mono 12";

// The command to be ran when you press alt + return
char term_cmd[] = "alacritty";

// The command to be ran when you press alt + b
char browser_cmd[] = "qutebrowser";

// The command that is ran when you press alt + e
char filemanager_cmd[] = "nautilus";

// Wether to wrap around client picker from begining to end or vice versa
const bool wrapClientPicker = false;

// Makes gtk windows stop rounding corners and also stops them from letting you resize them
const bool makeWindowsTile = true;

// Makes other windows such as alcritty stop drawing titlebars and resizing borders
const bool disableClientSideDecorations = true;

// Visual settings
const int text_horizontal_padding = 8;
const int text_vertical_padding = 3;
const int rounding_radius = 15;
float message_bg_color[4] = {0.156, 0.172, 0.203, 0.9};
float message_fg_color[4] = {1.0, 1.0, 1.0, 1.0};
