# lpd8editor

[![Join the chat at https://gitter.im/lpd8-editor/Lobby](https://badges.gitter.im/lpd8-editor/Lobby.svg)](https://gitter.im/lpd8-editor/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

A Linux editor for the [Akai LPD8 pad controller](http://www.akaipro.com/product/lpd8).

![lpd8-editor screenshot](doc/screenshot.png?raw=true "lpd8editor")

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
$ cd lpd8editor
$ qmake
$ make
$ make check
$ sudo make install
$ /usr/bin/lpd8editor
```

## Changing the installation prefix

``` sh
$ export INSTALL_PREFIX=/my/own/path
$ [build, test...]
$ make install
$ /my/own/path/bin/lpd8editor
```

## Build Debian package

```sh
$ dpkg-buildpackage -b -rfakeroot -us -uc
```

## Want to contribute a translation ?

It's actually easy and should not take more than a few minutes for a full translation. Please [get in touch on Gitter](https://gitter.im/lpd8-editor/Lobby) or create a new issue on GitHub.

## LPD8 sysex

Akai does not provide a MIDI and Sysex implementation sheet, so we [reverse engineered our own](doc/SYSEX.md).

