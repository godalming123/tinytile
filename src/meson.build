executable(
  meson.project_name(),
  ['main.c', 'popup_message.h', 'config.h'],
  dependencies: [
    wlroots,
    server_protos,
    wayland_server,
    xkbcommon,
    libinput,
    pangocairo,
  ],
  install: true,
)
