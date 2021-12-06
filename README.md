**slod** is a tiny local HTTP server written in C,
for serving static files with live reloading[^1].

## Installation

    make
    sudo make install

## Usage

    Usage: slod [options] [ROOT]
      -h, --help       Show this help message and quit
      -p, --port PORT  Specify port to listen on
      -l, --live       Enable livereload

## Issues

- No live reloading yet.

- If a client closes the connection too early and doesn't listen,
  we crash.

      curl -I localhost:8000


[^1]: Not yet, but it's already quite useful (to me).
