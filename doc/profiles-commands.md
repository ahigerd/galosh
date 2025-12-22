# Profiles - Commands Tab

The Commands tab in the Profiles dialog is used to create customized text commands, also known as "aliases."

![Screenshot of Commands tab](images/profiles-commands.png)

## Command list

On the left side of the Commands tab is the list of existing commands. Click on a row in this list to edit the associated command. You can switch
between commands without losing unsaved changes.

Click the New Command button to create a new command. Click the Delete Command button to remove the currently selected command.

## Command

This field defines what you type in the [command line](session-commandline.md) to run your custom command.

Commands may not contain spaces but may contain other characters. Commands may not start with `/` or `.` to avoid conflicts with slash commands or
speedwalk paths.

## Actions

The actions are the sequence of commands that are executed when the custom command is used.

To add an action to the list, type in an empty row. A new blank row will be created to ensure that there is always at least one blank row at the
bottom of the list.

To remove an action from the list, click the Remove button beside it.

Actions may include [slash commands](sessions-commands.md), [speedwalk paths](commands/speedwalking.md), and other custom commands. If any of these
are used, then the next action will not be run until the previous one is completed. Any other action is sent to the MUD directly, with no delay.

A command may not trigger itself with an action, not even indirectly. If an action matches its own command, or matches any command that triggered it,
it will be sent to the MUD as-is instead. For example, if a command `wake` had the actions `wake` and `stand`, the MUD will receive `wake` and
`stand` instead. This may be used to add additional behaviors to a normal MUD command. (_Note: This is currently broken. A command that references
itself will cause Galosh to hang._)

## Parameters

Custom commands may accept parameters. In the command line, parameters are separated from the command name by spaces. Quotation marks (`"`) can be
used to include a space inside a parameter.

Parameters will be substituted into all actions in the command. `%1` will be replaced by the first parameter, `%2` will be be replaced by the second
parameter, and so on. If fewer than the specified number of parameters are given, any additional references will be removed.

As an alternative to removing references to parameters, you may instead provide a default value using this format: `%{number:default}`. With this
syntax, if a parameter is omitted, the default value will instead be substituted into the action.

Add a `+` to a parameter reference to additionally include all parameters following it. For example, `%2+` will capture the second parameter as well
as any other parameters after it. `%*` is an alternative way to write `%1+`.

### Examples

* Command: `MyCommand`
* Action: `say %1`
    * Input: `MyCommand`
    * Output: `say `
    * Input: `MyCommand Hello, world!`
    * Output: `say Hello,`
    * Input: `MyCommand "Hello, world!"`
    * Output: `say Hello, world!`
* Command: `od`
* Action: `unlock %{1:door}`
* Action: `open %{1:door}`
    * Input: `od`
    * Output: `unlock door`, then `open door`
    * Input: `od gate`
    * Output: `unlock gate`, then `open gate`
* Command: `tbold`
* Action: `tell %1 *** %2+ ***`
    * Input: `tbold tspil Listen to me!`
    * Output: `tell tspil *** Listen to me! ***`

-----

[Back: Profiles - Triggers Tab](profiles-triggers.md) &bull; [Next: Profiles - Appearance Tab](profiles-appearance.md)
