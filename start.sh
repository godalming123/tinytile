#!/bin/sh
tinytile start\
  keyboardLayout   us\
  keyboardOptn     caps:escape\
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
