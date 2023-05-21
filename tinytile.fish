complete -c tinytile -f
complete -c tinytile -n "not __fish_seen_subcommand_from start about" -a "start about"
complete -c tinytile -n "__fish_seen_subcommand_from start" -a "keyboardLayout keyboardOptns termCmd disableCSD makeClientsTile minMargin moveClientsBy newClients font rounding bg fg verticalPadding horizontalPadding"
