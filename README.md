# Gluqlo: Fliqlo for Linux (SDL2)

Fork of Gluqlo with SDL2 support. Be sure to set the environment variable `SDL_VIDEO_ALLOW_SCREENSAVER=1` before starting the screensaver background daemon (xscreensaver, etc.).

## Background

Some Linux distros (ie. Arch) have recently replaced the `sdl` package with `sdl12-compat`, which attempts to wrap SDL2 functionality into SDL1.2. gluqlo no longer works as a result because `SDL_WINDOWID` is ignored by sdl12-compat. This environment variable no longer exists in SDL2, and screensaver programs running hacks using SDL1.2 need `SDL_WINDOWID` to properly display. Instead of waiting for sdl12-compat to add support for this environment variable I decided to migrate the gluqlo project to SDL2.
