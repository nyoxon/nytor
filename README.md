# nytor

**A terminal-based text editor written in C designed for programming.**

![nytor demo](assets/demo.gif)

`nytor` is a terminal-based text editor focused on being fast, lightweight, minimal and extensible, while providing a modern editing experience for linux only (for now).

The idea behind it was to create a terminal-based editor like vim or hex
but one that includes some of extra features, such as those found in editors
like subl; and above all, be an editor I would use on a daily basis.


## Features

### Syntax highlighting

Syntax highlighting is provided through plugins, allowing different languages to have their own syntax rules.

![Syntax highlighting](assets/highlight.gif)

Currently supported:

* C
* bash
* Make
* Rust *(planned)*

### Autocomplete

The editor provides word completion based on the contents of the current file.

For performance reasons, the use of the autocomplete is optional and must be
activated in the settings.

![Autocomplete](assets/autocomplete.gif)

### Unicode support

Text is internally represented using Unicode code points, allowing the program to work with UTF-8 text naturally.

### Multiple files

Open and switch between multiple files without leaving the editor.

![Multiple files](assets/multiple_files.gif)

### Search and replace

replace occurrences individually or throughout the entire file.

![Search and replace](assets/replace.gif)

### Plugins

Syntax highlighting and other functionality can be extended through plugins.

```text
plugins/
├── c/
├── rust/
└── ...
```

### Prompt command

In order to extend some of the keybind functionalities, there is a prompt
where you can enter specific commands.

You can open it by pressing Ctrl + N. Try tipping the `help` command.

### Other features

In addition to those presented above, the editor has several other features, such as:

- interactive window (Ctrl + Q to close it)
- goto (Ctrl + G or `goto` cmd)
- find a pattern (Ctrl + F or `find`/`match` cmds)
- create/rename a file (Ctrl + T or `new`/`saveas`/`open` cmds)
- call a shell command (`system` cmd)
- create a new (slave) shell section (Ctrl + N or `terminal` cmd)
- force synchronization (`sync` cmd)
- move the cursor to the beginning/end of the line (Ctrl + D or Alt + D)
- move selected lines up/down (Ctrl + Shift + Up/Down)
- undo/redo (Ctrl + Z/Y or `undo`/`redo` cmds)
- auto indentation (plugin only)
- auto comment a line/selection (Ctrl + K or `comment` cmd, plugin only)
- indent/unindent a line/selection (Ctrl + P/O or `indent`/`unindent` cmds)
- set/unset a file as readonly (`set` cmd)
- render a visual representation of spaces and tabs (Alt + T or `show` cmd)
- inotify handler
- (...)

### Possible future features

- Minimal integration with compilers
- Hexadecimal mode
- Create a separate window for the pty created by the `terminal` command
- `comment` cmd start operating on block comments as well

## Installation

### Dependencies

On Debian-based systems:

```bash
sudo apt install gcc make
```

### Building

Clone the repository and build:

```bash
git clone <repository>
cd nytor

make
```

Then run:

```bash
./nyt file.txt
```

or


```bash
./nyt -h
```

You can exit the program by pressing Ctrl + Q

### Installing

You can automatically install the program with a default
configuration, themes and plugins:

```bash
make
sudo make install
```

### Uninstalling

In order to uninstall the program:

```bash
sudo make uninstall
```

## Usage

```bash
nyt [ARGS] [FILE...]
```

If no file is provided, the program starts with an empty buffer.

## Manual

See how to use the editor, change settings, create themes and more on the man page:

```bash
man manual/nyt.1
```

or

```bash
man nyt
```

## Configuration

Configuration files are stored in:

```text
~/.config/nytor/
```

## Project status

`nytor` is currently under active development.

There are a few "incomplete" aspects to the program, mainly regarding
the built-in autocomplete (because i didn't find a very intuitive way to implement it, which is one of the reasons it is optional).
However, i believe the project is at a suitable level to actually be
used for programming.

## Contributing

Contributions, bug reports and suggestions are welcome.

## License

This project is licensed under the GNU General Public License v3.0.
See the [LICENSE](LICENSE) file for details.
