# REMOTE MPV
Basic HTTP server which handles POST requests and forward their content to a UNIX socket

## Build and run

### Requirements
- C compiler
- libc
- libmagic
- support for unix sockets

### to build
```sh
make build
```

### to run
first launch mpv with socket ipc :
```sh
mpv --profile=pseudo-gui --input-ipc-server=/tmp/mpvsocket --idle
```
then launch the server with :
```sh
make run
```
or with: 
```sh
build/out/remote-mpv -p 8080 -s /tmp/mpvsocket -d web
```
