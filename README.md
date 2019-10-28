# Synopsis

Minimal REST API demo for TI's CC3200 WiFi chip (on development board)
with instructions for compiling and debugging on Linux.

# Usage

Build instructions on Linux systems

1. Setup path to CC3200 SDK as documented in the Linux Setup section
   below
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
5. Send `GET` and `POST` in the Wifi Demo API section

# Wifi Demo API

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


# Linux Setup

## Systems used

I used following systems for compiling and initial development:

- Fedora 24 (4.11.12-100.fc24.x86_64)
- OpenSUSE Tumbleweed (4.12.8-1-default (4d7933a) x86_64)
  - installed packages (described below)
    - `libncurses5-32bit`
    - `openocd` (dependencies: `libftdi1-2 libhidapi-hidraw0 libjaylink libjaylink0 libjim0_75 libusb-0_1-4 openocd openocd-data libjaylink`)
    - `libusb-1_0-devel`
- CC3200SDK v1.3.0
- gcc-arm-none-eabi 5.4 2016 q3

## Instructions

1. Download CC3200 SDK
- Download from TI website
  https://www.ti.com/tool/cc3200sdk
  verison 1.3.0 was used at the time of this writing
- Run installer (ran under a Windows machine) and copy the installed
  directory. All required files are bundled in the installed
  directory)
- Copy content to `/path/to/directory/goes/here`. For convenience,
  assign a variable to this path:
  ```bash
  echo export CC3200SDK=/path/to/directory/goes/here >> ~.bashrc
  ```
  and reload your `.bashrc`:
  ```bash
  . ~/.bashrc
  ```

2. Configure LaunchXL JTAG Debug interface
- Load `ftdi-sio` module
  ```bash
  modprobe ftdi-sio
  echo 0451 c32a > /sys/bus/usb-serial/drivers/ftdi_sio/new_id
  ```
- If running an old kernel, syntax will be different; see references above

3. Build and Install openocd
- Fedora/CentOS
  ```bash
  yum install libusb-devel
  ```
- OpenSUSE
  ```bash
  zypper in libusb-1_0-devel
  ```
- OpenOCD comes in default OpenSUSE repos:
  ```bash
  zypper in openocd
  ```

For OpenSUSE go to next step. For Fedora: download openocd, extract, and `cd` into it then
  ```bash
  ./configure
  ```

  Make sure `configure` reports `MPSSE mode of FTDI based devices yes (auto)`. Then build:

  ```bash
  make -j 4
  make install
  sed -i s/plugdev/YOUR_GROUP_NAME_GOES_HERE/g contrib/60-openocd.rules
  cp openocd/contrib/60-openocd.rules /etc/udev/rules.d/
  ```

  (Note: rules do not affect devices that are already plugged
  in. If the Launchpad was plugged in, unplug and replug it to take effect)
- Make sure `openocd` runs
  ```bash
  openocd -f $CC3200SDK/tools/gcc_scripts/cc3200.cfg
  ```
  Press `C-c` to exit

4. Install cross tool chain
- In Fedora, tool chain is available in the official repos:
 ```bash
 yum install arm-none-eabi arm-none-eabi-newlib arm-none-eabi-gdb
 ```
 For OpenSUSE, build or download the prebuilt toolchain binaries
 (https://launchpad.net/gcc-arm-embedded/+download) and put the
 binaries in path, e.g.
 ```bash
 echo export PATH=$PATH:/opt/gcc-arm-none-eabi-5_4-2016q3/bin/ >> ~.bashrc
 ```
 and reload `.bashrc`
- Edit `$CC3200SDK/tools/gcc_scripts/gdbinit` to make sure OpenOCD finds its configuration file:

  ```bash
  target remote | openocd -c "gdb_port pipe; log_output openocd.log" -f $CC3200SDK/tools/gcc_scripts/cc3200.cfg
  ```

## Flashing
[cc3200tool](https://github.com/ALLTERCO/cc3200tool) can be used for
flashing and downloading files. Its README is self-explanatory.

## Compile and Run Examples

### Example 1
Building `$CC3200SDK/example/blinky`

- Jumpers need to be set correctly, see Figure 1 in the
 `Getting_Started_Guide.pdf` document
 ```bash
 cd $CC3200SDK/example/blinky/gcc
 make
 ```

- Run a GDB session with the built file
 ```bash
 arm-none-eabi-gdb -x $CC3200SDK/tools/gcc_scripts/gdbinit exe/blinky.axf
 ```

Alternatively, you may consider adding this alias to your `.bashrc`:
  ```
  echo alias gdb-cc3200='arm-none-eabi-gdb -x $CC3200SDK/tools/gcc_scripts/gdbinit' >> ~.bashrc
  ```
  and starting the debugger with
  ```bash
  gdb-cc3200 exe/blinky.axf
  ```
- Press `c` and `enter` to continue executing the `main` function

### Example 2
Building `$CC3200SDK/example/getting_started_with_wlan_ap`

This example requires input from the user, which is written to a tty

- Jumpers need to be set correctly, and board must be in AP Mode. See
  `/docs/example/` documents
- Build the application
  ```bash
  cd $CC3200SDK/example/getting_started_with_wlan_ap/gcc
  make
  ```
  Concatenate USB0 tty (keep it running)
  ```bash
  cat /dev/ttyUSB0
  ```

  This of courses assumes that you have a single `/dev/ttyUSB0`
  corresponding to the development board.

- Run a GDB session with the built file in a separate terminal
  ```bash
  arm-none-eabi-gdb -x ~/cc3200-sdk/tools/gcc_scripts/gdbinit exe/wlan_ap.axf
  ```
  and press `c` to continue

- Open another terminal and send in the SSID when requested:
  ```bash
  echo "this is my ssid" > /dev/ttyUSB0
  ```

Alternatively, instead of `cat`ing the controlling terminal, you can
use `screen`:

- Get the baudrate of the tty
 ```bash
 stty < /dev/ttyUSB0
 ```
- Open screen
 ```bash
 screen /dev/ttyUSB0 insert_baudrate_here
 ```
 and replace `insert_baudrate_here` with the actual baudrate

### Example 3
Building `$CC3200SDK/example/tcp_socket`

This example shows how to run the `tcp_socket` example with only the tools that come shipped with every distro.

Sending packets

- Overwrite `SSID_NAME`, `SECURITY_TYPE`, and `SECURITY_KEY` in
  `main.c` with the information from your router
- Build the application
  ```bash
  cd $CC3200SDK/example/tcp_socket/gcc
  make
  ```
- Concatenate USB0 FIFO (keep it running)
  ```bash
  cat /dev/ttyUSB0
  ```
- Run a GDB session with the built file in a separate terminal
  ```bash
  arm-none-eabi-gdb -x ~/cc3200-sdk/tools/gcc_scripts/gdbinit exe/tcp_socket.axf
  ```
  and press `c` to continue
- Follow as in previous example to make selections in prompted
  options. Make following changes:
  - Packet size: 10

  This change is made to make the example complete quicker when
  manually sending packets
- Navigate the menus and make CC3200 listen for packets
- Start a telnet connection to the CC3200
  ```bash
  telnet <LOCAL_IP_ADDRESS_OF_CC3200> 5001
  ```
  and start typing random strings followed by enter. Repeat 10 times.

Receiving packets:
- Follow as in previous example and make following selections:
  - Packet size: 10
  - Destination IP: Local IP address of your computer
  - Port: 5001
- Start a server on your local machine
  ```bash
  nc -vl <LOCAL_COMPUTER_IP_ADDRESS> 5001
  ```
- Navigate the menus on CC3200 and set it to send packets to server
  (your computer)


## Reference
1. http://azug.minpet.unibas.ch/~lukas/bricol/ti_simplelink/CC3200-LaunchXL.html
2. https://community.particle.io/t/cc3200-network-processor-information-station/5348/59
3. https://hackpad.com/Using-the-CC3200-Launchpad-Under-Linux-Rrol11xo7NQ
