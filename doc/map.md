# Mapping

## Collecting map data

Galosh's automapper collects data as you navigate the MUD world to construct an interactive map. This requires no special action on your part. The
automapper will remember where you have visited and build the map based on what you have seen.

### GMCP

The primary source of information used by Galosh's automapper is the [Generic MUD Communication Protocol](https://mudstandards.org/mud/gmcp) (GMCP).

The `room` GMCP package provides the numeric room ID, room name, zone name, terrain type, and exits. Most MUDs that implement GMCP will send this
package each time you enter a room.

### Descriptive text

After moving between rooms, Galosh will read the description of the destination room from the displayed text. It will also update the room
description if you use a command such as `LOOK`.

If Galosh detects an "Obvious exits:" message like the output of the `EXITS` commands on many MUDs, it will use that information to add more data
to the surrounding rooms.

Galosh will attempt to collect room names, descriptions, and exits from the displayed text on MUDs that do not use GMCP.

### Mudlet maps

If the MUD provides a link to a Mudlet map using the `client.map` GMCP package, Galosh will download the file and incorporate it into its map data.

To avoid unnecessarily redownloading maps, Galosh will not download a map file that it has downloaded within the last 24 hours. Galosh will also check
the `Last-Modified` header before downloading the file. If the file has not been updated since the last time it was downloaded, Galosh will skip it.

Imported maps have a lower priority than other sources of map data. If information in a Mudlet map disagrees with existing data, the existing data
will be preserved.

Galosh only uses room and exit information from Mudlet maps. Because Galosh has its own map layout system, it does not use any layout information
from imported maps.

## Mapping features

The mapping system in Galosh uses an automatic layout system to position each room on the map.

In the main window, Galosh can display a map of the current zone in the [mini-map](session-docks.md#mini-map) and additional information about the
current room in the [room description panel](session-docks.md#room-description).

The [Map Explorer](map-explorer.md) can browse zone maps and inspect details about rooms, even without a connection to the MUD. You can use the Map
Explorer to plan routes and review information.

Galosh has a [pathfinding](commands/route.md) feature that can calculate the best route between rooms, and you can set [waypoints](map-waypoints.md)
to keep track of important locations and quickly travel to them.

## Caveats

GMCP is the most reliable source of map data. While Galosh will attempt to construct map information without it, this is less reliable. You may need
to correct errors in the map data on MUDs that do not use GMCP.

The automapper assumes that information about the map does not change after it is collected. This allows it to remember secret doors once they are
discovered, but it also means that it may become confused if changes occur. Zones with in-game effects that cause rooms to move or exits to connect
to different places are not suitable for automatic mapping.

-----

[Back: Map Explorer - Commands Overview](map-commands.md) &bull; [Up: Table of Contents](index.md) &bull; [Next: Waypoints](map-waypoints.md)
