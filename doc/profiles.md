# Profiles

Galosh organizes settings into profiles. Each profile represents a single character in a server or program.

The Profiles dialog is the first thing presented when opening Galosh. It is used to manage and connect to profiles.

All settings contained within the Profiles dialog are specific to a single profile. However, [maps](map.md), [items](itemdb.md), and
[SSL / TLS security certificates](session-tls.md) are shared between all profiles using the same
[hostname or command](profiles-server.md#connect-to-a-server).

![Screenshot of Profiles dialog](images/profiles-server.png)

## Profile List

On the left side of the Profiles dialog is the list of available profiles. Select a profile from the list to view and edit its settings. If you have
unsaved changes when switching profiles, you will be prompted to save them.

Click the New Profile button below the list to create a new profile. Click the Remove Profile button to delete the currently selected profile.

## Tabs

Each tab manages a different group of settings for the selected profile:

* [Server](profiles-server.md): Configures how to connect to the server / program
* [Triggers](profiles-triggers.md): Configures triggers that automatically respond to incoming text
* [Commands](profiles-commands.md): Configures user-defined text commands
* [Appearance](profiles-appearance.md): Configures the font and color scheme for the selected profile

## Actions

When the Profiles dialog is opened on startup or using the Connect function, it will present four action buttons:

* Connect
* Offline
* Close
* Apply

When it is opened using the Profiles, Triggers, or Commands functions, it will present three:

* OK
* Close
* Apply

### Connect

This button will save any outstanding changes to the current profile, close the Profiles dialog, and open a new session that connects to the server
or program.

Only one session can be open for a profile at a time. If you wish to have two connections to a server, create two profiles.

### Offline

This button will save any outstanding changes to the current profile, close the Profiles dialog, and open a new session in
[offline mode](sessions-offline.md).

Only one session can be open for a profile at a time. If you wish to have two sessions open simultaneously, create two profiles.

### OK

This button will save any outstanding changes to the current profile and close the Profiles dialog without opening any new sessions.

### Close

This button will close the Profiles dialog without opening any new sessions. If you have unsaved changes, you will be prompted to save them.

### Apply

This button will save any outstanding changes to the current profile without closing the Profiles dialog.

-----

[Back: Introduction](intro.md) &bull; [Up: Table of Contents](index.md) &bull; [Next: Profiles - Server Tab](profiles-server.md)
