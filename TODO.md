## Bugs / incomplete implementation

* Commands tab doesn't seem to have proper dirty detection
* Validate that commands don't start with `/` or `.`
* Fix escaping in custom command args, remove quotes
* Make parsing multiline input use the command queue
* "Revert Scheme" button doesn't update when creating a new scheme
* Decide on a better default font strategy
* Title bar doesn't clear after closing last session (and maybe other things?)
* Map explorer opens with the wrong zone selected in the dropdown

## Polish

* Add tooltips to all icons
* Make Triggers and Commands UIs consistent (probably use Triggers design?)
* Use new doc icon instead of new folder icon for New Profile
* Check for consistency in "remove" vs "delete"
* Double-check consistency on "..." in menu items and button labels
* Remove "command aborted due to error" message when not executing a custom command
    * Maybe also remove it when it's the last command in the list?
* Error out speedwalk commands in offline mode instead of timing out

## Incomplete features

* Custom command arg validation options? (or autodetect?)
* Detect recursive commands
* Make multiline commands honor async
* Add some kind of escape for `.`

## New features

* Command to toggle trigger enablement
* /ECHO command
* Prevent navigating into unmapped rooms in the explorer
* Automap disable prefs (per zone, per server, command toggle)
* Expose Mudlet import in UI
    * Show message in scrollback if map updated on connect
* Capture room descriptions from GMCP when present
* Disable automapping when room ID is -1
* Door rendering in mapper
* Option to clear recorded room information to recapture
* Manual map editing
* Feature to highlight unexplored areas?
