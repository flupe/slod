**slod** is a tiny opinionated local HTTP server written in C,
for serving static files with live reloading[^1].

## Features

- Tiny executable with no dependencies.
- Directory listing, where files are sorted alphabetically **and** such that
  `1-file.txt` will still appear *before* `11-file.txt`.
- Tells the browser to **not** cache anything.
- Shows `index.html` instead of directory listing if it exists.

## Installation

    make
    sudo make install

## Usage

    Usage: slod [options] [ROOT]
      -h, --help       Show this help message and quit
      -p, --port PORT  Specify port to listen on
          --no-hidden  Do not show hidden files on directory index
      -l, --live       Enable livereload

## Issues

- No live reloading yet.
- If a client closes the connection too early and doesn't listen,
  we crash.

      curl -I localhost:8000


[^1]: Not yet, but it's already quite useful (to me).
