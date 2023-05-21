#!/bin/sh
# Note that when options need to have a space in them then you should instaed use an underscore
tinytile start\
  keyboardLayout   us\
  keyboardOptns    caps:escape\
\
  termCmd           alacritty\
  disableCSD        yes\
  makeClientsTile   no\
  minMargin         30\
  moveClientsBy     45\
  newClients        float\
\
  font              Mono_12\
  rounding          12\
  bg                000000\
  fg                FFFFFF\
  verticalPadding   3\
  horizontalPadding 8\
