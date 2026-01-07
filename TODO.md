## Bugs / unfinished

* Commands tab doesn't seem to have proper dirty detection
* Validate that commands don't start with `/` or `.`
* Validate that waypoint names don't contain spaces
* "Revert Scheme" button doesn't update when creating a new scheme
* Decide on a better default font strategy
* Title bar doesn't clear after closing last session (and maybe other things?)
* Map colors/costs might be getting clobbered
* Parsing button broke (menu item still works)
* Apply button in profiles window doesn't clear dirty flag

## Polish

* Add tooltips to all icons
* Make Triggers and Commands UIs consistent (probably use Triggers design?)
    * And maybe also Waypoints?
* Use new doc icon instead of new folder icon for New Profile
* Check for consistency in "remove" vs "delete"
* Double-check consistency on "..." in menu items and button labels
* Remove "command aborted due to error" message when not executing a custom command
    * Maybe also remove it when it's the last command in the list?
* Error out speedwalk commands in offline mode instead of timing out
* Show message in scrollback if map updated on connect
* Bind QKeySequence::HelpContents to show help
    * Maybe have a help browser in addition to text-based help?
* Add vnum to `/WAY` output
* Use `\t` for table formatting in in-app help text
    * Detect display width for wordwrapping?
* Some table views use bold headers, others don't
* Decide on "room type" vs "terrain type"
* Verify button icons on other platforms (or create custom icons for everything)

## Incomplete features

* Custom command arg validation options? (or autodetect?)
* Detect recursive commands
* Customizable speedwalk prefix
* Double-click minimap to open map explorer
* Add right-click menu in mini-map/map explorer
* Check user-defined terrain types when heuristically coloring non-GMCP rooms
* Expose some map commands to main window

## New features

* Command to toggle trigger enablement
* /ECHO command
* Prevent navigating into unmapped rooms in the explorer
* Automap disable prefs (per zone, per server, command toggle)
* Expose Mudlet import in UI
* Capture room descriptions from GMCP when present
* Disable automapping when room ID is -1
* Door rendering in mapper
* Option to clear recorded room information to recapture
* Manual map editing
* Feature to highlight unexplored areas?
* Fullscreen mode?
* Right-click menu in scrollback
* Replace newlines with `|` when pasting in command line (except at ends of string)
* Support using waypoints with `.`
* Maybe option to make `.` fast by default?
* Accessibility
* "Duplicate Profile" button
* Custom commands in map editor
* Avoid individual rooms for routing
