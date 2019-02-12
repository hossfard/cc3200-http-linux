# Summary

Build instructions on Linux systems

1. Setup path to CC3200 SDK as documented in `linux_setup.md`
2. Build `http-server.axf`
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
   make
   ```
   You may need to run the toolchain and build twice due to an ongoing
   problem in cmake instructions.
2. Run using `gdb`
   ```bash
   arm-none-eabi-gdb -x $CC3200SDK/tools/gcc_scripts/gdbinit http-serve.axf
   ```
3. The board will start in AP mode with SSID `Wifi Demo` (open)
4. Connect to the chip
5. Send `GET` and `POST` as described below

# API

Server: 192.168.1.1 (`wifidemo.com`)
Port: 5001

## `/led`

### `GET`

Return Status of the three LED lights (red, green, and orange) as a
JSON object.

Method: `GET`
Path: `/led`

Example request:

```bash
curl 192.168.1.1:5001/led
```

Example response:
```
200 OK HTTP/1.0
Content-Length: 45
Content-Type: application/json
{"orange": "off","green": "off","red": "off"}
```

### `POST`

Toggle LED lights `on` or `off`.

Method: `POST`
Path: `/led`
Content-Type: `application/json`

_Must_ specify `Content-Length`. Server may respond with 411 or 415 if
an invalid content-length or content-type, respectively, is specified.

Example request:

```bash
curl -H "Content-Type: application/json" -X POST -d \
    "{\"green\": \"on\", \"orange\": \"on\", \"red\": \"on\"}" 192.168.1.1:5001/led
curl -H "Content-Type: application/json" -X POST -d \
    "{\"green\": \"on\"}" 192.168.1.1:5001/led
curl -H "Content-Type: application/json" -X POST -d \
    "{\"red\": \"on\", \"green\": \"off\"}" 192.168.1.1:5001/led
```

Example response:
```
204 No Content HTTP/1.0
```
