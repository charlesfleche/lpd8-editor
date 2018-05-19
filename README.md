# lpd8-editor

<img src="lpd8-editor.svg?sanitize=true" width="64" height="64" /> [![Join the chat at https://gitter.im/lpd8-editor/Lobby](https://badges.gitter.im/lpd8-editor/Lobby.svg)](https://gitter.im/lpd8-editor/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

A Linux editor for the [Akai LPD8 pad controller](http://www.akaipro.com/product/lpd8).

![lpd8-editor screenshot](doc/screenshot.png?raw=true "lpd8-editor")

## Dependencies

- alsa
- c++11
- pkg-config
- qt5
  - QtSql
  - QtSVG
  - QtWidgets

## Build, test and run

``` sh
$ cd lpd8-editor
$ qmake
$ make
$ make check
$ ./lpd8-editor
```

## Usage

First, you should connect lpd8-editor to the hardware lpd8 with qjackctl or catia, (or other equivalent).
Then, when you are happy with a setting, go to the Lpd8 menu, then select the memory you want to store the preset to.


## Build Debian package

```sh
$ dpkg-buildpackage -b -rfakeroot -us -uc
```

## Want to contribute a translation ?

It's actually easy and should not take more than a few minutes for a full translation. Please [get in touch on Gitter](https://gitter.im/lpd8-editor/Lobby) or create a new issue on GitHub.

## LPD8 sysex

Akai does not provide a MIDI and Sysex implementation sheet, so we [reverse engineered our own](doc/SYSEX.md).
