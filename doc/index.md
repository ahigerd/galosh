# Galosh

This documentation applies to v0.1.0.

## Table of Contents

* [Introduction](intro.md)
* [Profiles](profiles.md)
    * [Server](profiles-server.md)
        * [MSSP](profiles-server.md#mssp)
    * [Triggers](profiles-triggers.md)
    * [Commands](profiles-commands.md)
    * [Appearance](profiles-appearance.md)
* [Main Window](session.md)
    * [Menus](session-menus.md)
        * [File](session-menus.md#file)
        * [View](session-menus.md#view)
        * [Tools](session-menus.md#tools)
        * [Help](session-menus.md#help)
    * [Toolbar](session-menus.md#toolbar)
    * [Character Stats](session-stats.md)
    * [Room Description](session-room.md)
    * [Mini-Map](session-minimap.md)
    * [SSL / TLS](session-tls.md)
    * [Offline Mode](session-offline.md)
    * [Commands](session-commands.md)
        * [Speedwalking](commands/speedwalking.md)
        * [/DC](commands/dc.md): Disconnects from the game
        * [/EQUIP](commands/equip.md): Switches equipment sets, or captures a snapshot of your equipment
        * [/EXPLORE](commands/explore.md): Opens the map exploration window
        * [/HELP](commands/help.md): Shows information about a command
        * [/ROUTE](commands/route.md): Calculates the path to a specified room
        * [/WAYPOINT](commands/waypoint.md): Adds, views, removes, or routes to waypoints
* [Map Explorer](map-explorer.md)
    * [Menus](map-menus.md)
        * [Map](map-menus.md#map)
        * [View](map-menus.md#view)
        * [Help](map-menus.md#view)
    * [Commands](map-commands.md)
        * [Speedwalking](commands/speedwalking.md)
        * [/BACK](commands/back.md): Returns to the previous room
        * [/GOTO](commands/goto.md): Teleports to a room by numeric ID
        * [/HELP](commands/help.md): Shows information about a command
        * [/HISTORY](commands/history.md): Shows the exploration history
        * [/RESET](commands/reset.md): Clears the exploration history
        * [/REVERSE](commands/reverse.md): Shows backtracking steps for the exploration history
        * [/ROUTE](commands/route.md): Calculates the path to a specified room
        * [/SEARCH](commands/search.md): Search for rooms by name and description
        * [/SIMPLIFY](commands/simplify.md): Removes backtracking from exploration history
        * [/SPEED](commands/speedwalking.md#speed): Shows speedwalking directions for the exploration history
        * [/WAYPOINT](commands/waypoint.md): Adds, views, removes, or routes to waypoints
        * [/ZONES](commands/zones.md): List known zones, or show information about a zone
* [Mapping](map.md)
* [Item Database](itemdb.md)
    * [Equipment Sets](itemdb-sets.md)
* Technical Information
    * [Configuration Files](config.md)
    * [Terminal Emulation](terminal.md)

## License

Copyright &copy; 2025 Adam Higerd

Galosh is free software; you can redistribute it and/or modify it under the terms of version 3 of the GNU General Public License as published by
the Free Software Foundation. This program is distributed in the hope that it will be useful, but without any warranty, even the implied warranty
of merchantability or fitness for a particular purpose. See [the full text of the GPLv3](../LICENSE.md) for more information.

Galosh makes use of portions of [QTermWidget](https://github.com/lxqt/qtermwidget). QTermWidget is available under the GPLv2 or any later version.
Some source files may be available under a different license. See each file in [src/qtermwidget](../src/qtermwidget) for more information.

On Windows, Galosh uses [mman-win32](https://github.com/alitrack/mman-win32), available under the [MIT license](../src/mman-win32/LICENSE.MIT).

Galosh is built upon Qt. Qt is copyright &copy; The Qt Company Ltd and other contributors and used under the terms of the GNU Lesser General Public
License version 3. See [Qt's licensing page](https://www.qt.io/licensing/open-source-lgpl-obligations) for more information.
