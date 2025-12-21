# Introduction to Galosh

## Why Galosh?

[Galoshes](https://en.wikipedia.org/wiki/Galoshes) are what you wear when you're going to go out in the mud.

Galosh is inspired by the simplicity of older MUD clients. [Mudlet](https://www.mudlet.org/) is nice &mdash; it's full-featured and well-supported.
And Galosh agrees with Mudlet's vision that the more projects are involved in the MUD ecosystem, the better. So if you find 3D map displays, fancy
scripts, and the like to be a little too much, Galosh may be right for you.

And maybe you'll find something in Galosh that you like. Galosh's tab completion works a little differently, its automapper keeps track of room
descriptions, and it can keep more information about the current room visible at a glance.

## Getting Galosh

### Windows

Galosh is distributed as a portable app on Windows. Download the [latest release of Galosh](https://github.com/ahigerd/galosh/releases/latest) and
extract `galosh.exe` to any path you like. No installation required!

### Build from source

Galosh requires Qt 5.15 or higher, including Qt 6.x.

* `git clone https://github.com/ahigerd/galosh.git`
* Open the `galosh` folder
* Optionally, edit `galosh.pro` to set the `CONFIG` flags.
    * The official Windows build uses `CONFIG += release static` and `CONFIG -= debug debug_and_release`.
* `qmake`
* `make`

## Using Galosh

### Quick Start

1. Launch Galosh. The Profiles dialog will open automatically.
2. Fill in a profile name, server hostname, and server port number. Check "Use SSL/TLS" if the server supports secure connections.
3. Optionally, fill in a username, password, and login/password prompts to automatically log in after connecting.
4. Click Connect.

-----

[Back: Table of Contents](index.md) - [Next: Profiles](profiles.md)
