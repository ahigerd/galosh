# Offline Mode

There are two ways that a session can be in offline mode:

* The connection to the server was closed, or the program associated with the session exited.
* The session was opened in offline mode using the "Offline" button in the [Profiles dialog](profiles.md).

A session in offline mode can be identified by the ![red](../res/red.png) red icon in its tab.

In offline mode, most Galosh features continue to work:

* [Triggers](profiles-triggers.md) and [custom commands](profiles-commands.md) can be tested offline, as long as they don't require a response from
   the MUD. (_Tip: Use the [/ECHO](commands/echo.md) command to test triggers._)
* [Appearance](profiles-appearance.md) settings will be applied to open sessions, whether online or offline.
* The [Map Explorer](map-explorer.md) always uses recorded automap data, even in online mode.
* [Waypoints](map-waypoints.md) can be viewed and modified.
* [Item Search](itemdb.md#search) always uses the local item database, even in online mode.
* [Equipment Sets](itemdb-sets.md) can be viewed and modified, but not equipped.
* [Help text](commands/help.md) for built-in commands can be read.

However, features that require communication with the MUD will not work:

* Text entered in the command line will be displayed on screen but will otherwise have no effect.
* Speedwalking in the main window will display an error unless done with `/SPEEDWALK -f`.
* The `/EQUIPMENT -u` command cannot be used to collect updated equipment information.
* The [dockable panels](session-docs.md) will not be updated.
* The "View MSSP Info..." menu item and the MSSP button in the toolbar will show the last-known status collected from the server.
    * If the session was opened in offline mode, the MSSP button will be unavailable.
    * The [MSSP button](profiles-server.md#mssp) in the Profiles dialog can be used to make a new connection to the server to collect up-to-date MSSP
      information.

If a valid hostname and port or a valid command line are configured for the associated profile, an offline session can become an online session using
the "Reconnect" button or menu item.

-----

[Back: SSL / TLS](session-tls.md) &bull; [Up: Table of Contents](index.md) &bull; [Next: Main Window - Commands Overview](session-commands.md)
