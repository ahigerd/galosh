# Mapping

Galosh has a powerful automapper that can combine information from multiple sources into a detailed map.

## How does it work?

### GMCP

The primary source of information used by Galosh's automapper is the [Generic MUD Communication Protocol](https://mudstandards.org/mud/gmcp) (GMCP).

The `room` GMCP package provides the numeric room ID, room name, zone name, terrain type, and exits.

### Descriptive text

After moving between rooms, Galosh will read the description of the destination room from the displayed text. It will also update the room
description if you use a command such as `LOOK`.

If Galosh detects an "Obvious exits:" message like the output of the `EXITS` commands on many MUDs, it will use that information to add more data
to the surrounding rooms.

Galosh will attempt to collect room names, descriptions, and exits from the displayed text on MUDs that do not use GMCP. However, this is less
reliable, and you may need to correct errors in the map data afterward.

### Mudlet maps

If the MUD provides a link to a Mudlet map using the `client.map` GMCP package, Galosh will download the file and incorporate it into its map data.

To avoid unreasonably redownloading maps, Galosh will not download a map file that it has downloaded within the last 24 hours. Galosh will also check
the `Last-Modified` header before downloading the file. If the file has not been updated since the last time it was downloaded, Galosh will skip it.

Imported maps have a lower priority than other sources of map data. If information in a Mudlet map disagrees with existing data, the existing data
will be preserved.

Galosh only uses room and exit information from Mudlet maps. Because Galosh has its own map layout system, it will not use any layout information
from imported maps.

## What can it do?

The mapping system in Galosh uses an automatic layout system to position each room on the map.

In the main window, Galosh can display a map of the current zone in the [mini-map](session-docks.md#mini-map) and additional information about the
current room in the [room description panel](session-docks.md#room-description).

The [Map Explorer](map-explorer.md) can browse zone maps and inspect details about rooms, even without a connection to the MUD. You can use the Map
Explorer to plan routes and review information.

Galosh has a [pathfinding](commands/route.md) feature that can calculate the best route between rooms, and you can set [waypoints](map-waypoints.md)
to keep track of important locations and quickly travel to them.

-----

[Back: Map Explorer - Commands Overview](map-commands.md) &bull; [Up: Table of Contents](index.md) &bull; [Next: Map Settings](map-settings.md)
