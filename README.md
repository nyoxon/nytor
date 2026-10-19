# nytor

**A terminal-based text editor written in C designed for programming.**

![nytor demo](assets/demo.gif)

`nytor` is a terminal-based text editor focused on being fast, lightweight, 
minimal and somehow extensible, while providing a modern editing experience for 
linux only (for now).

The idea behind it was to create a terminal-based editor 
like vim or hex (which i don't like because they are horrible to use)
but one that includes some of extra features, such as those found in editors
like sublime text (which i don't like to getting pop-ups asking me to buy it); 
and above all, be an editor I would use on a daily basis.


## Why use nytor

Actually, there's no reason at all, given that any mainstream
editor does what `nytor` does, and likely does it better or
more efficiently.

## Features

### Syntax highlighting

Syntax highlighting is provided through plugins, allowing different languages to have their own syntax rules.

![Syntax highlighting](assets/highlight.gif)

Currently supported:

* C
* bash
* Make
* Lua
* Rust
* Assembly x86
* GDScript

There is a poorly documented API ([here](include/plugins/language_syntax.h))
that someone could use in order to create their own lexer.

### Autocomplete

The editor provides word completion based on the contents of the current file.

The use of the autocomplete is optional and is disabled by default,
but you can enable it using the config file.

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
├── lexers/
│   ├── c/
│   └── rust/
└── ...
```

The lexer sources are kept under `plugins/lexers/`; `make plugins` builds them
under `build/plugins/lexers/`.

### Prompt command

In order to extend some of the keybind functionalities, there is a prompt
where you can enter specific commands.

You can open it by pressing Ctrl + N. Try tipping the `help` command.

### Lua plugins/scripts (Experimental)

Programs written in Lua can be called directy from the editor.
There is no divine reason for using Lua (rather than, say, C,
which is already used for a large portion of the prompt commands
and lexer plugins); it's simply because i've never used two
languages in a single project and would like to give it
a try.

Actually, it would be interesting (not necessary) to be able
to extend the editor's functionaly without having to recompile
it every single time, especially since i don't intend to
create a billion commands for the prompt (there are already
a plenty) or turn my editor into a monster like C++

Naturally, there's an API of defined functions ([here](src/plugins/lua_plugin.c))
that a Lua script must use to communicate with the editor's
internal structures.

There are two ways to call a script:

- Using the `lua` command inside the prompt.
- Adding it to `nytor`/plugins/ directory and calling it
by its name in the same way that you would call a builtin
editor command (autocomplete will even work for it)

Since this is an experimental feature, you don't even
need to have the Lua interpreter installed on your system,
as it's loaded dynamically into the editor process's memory.
See some examples of scripts [here](plugins)

#### Poins to consider

- Must use Lua 5.2 or newer.
- Dynamically loading the interpreter introduces a tiny
but noticeable delay immediately after a script is called.
- This is an experimental feature, which means that
absolutely everything about it could change, many erros
could exist, or even be removed in later versions.


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
- `comment` cmd start operating on block comments as well
- The `find` functions accept regular expressions
- goto-definition operations etc (see [gotod](scripts/gotod.sh))

## Installation

### Dependencies

On Debian-based systems:

```bash
sudo apt install gcc make
```

In order to use your system's clipboard, you'll need:

#### Wayland:

```bash
sudo apt install wl-clipboard
```

#### X11:

```bash
sudo apt install xclip
```

or

```bash
sudo apt install xsel
```

None of this is really necessary, as the editor has an internal clipboard, but it's required if you want to copy text from within the editor directly to your system clipboard and vice versa.

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

There is also a .deb package that installs the program (check Releases section).

## Default config file, themes and plugins

The `nytor` directory, which houses the config file, themes,
plugins, etc, must be located in one of these three directories
(in fact, the editor checks for the directory's existence in
precisely this order):

- $HOME/.config/
- /usr/local/share/
- /usr/share/

More specifically, the editor will search for the `nytor` directory
following the order above and will use the first one that exists.
This means there's no such thing as "scanning all directories" for
example, to find plugins or theemes, but rather only "scanning
a single directory". If HOME/.config/nytor exists, any plugins,
themes, or config file that might otherwise be found in
other directories will be completely ignored. The reason behind
this is that, ideally, I would prefer for the directory
to exist only within $HOME. However, since i also want to use
`make install` and distribute a `.deb` package, both of which
require privileged permissions, it would be a bit odd
to create directories within the user space. If you don't like
this, feel free to create you own version of the editor (see [license](LICENSE)).

When using `make install` to install the program, the default
configuration files will be located in `/usr/local/share/nytor/`.
If installed via the .deb package, they will be in
`/usr/share/nytor/`.

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

## Limitations

- CRLF are normalized to LF

- The editor assumes it is dealing with seekable files (ex: regular files).

- There is no hex mode.

- There is no tilde expansion (inside the prompt).

- The program currently doesn't handle huge files very well: that is,
it will use a lot of memory if that happens.

- To avoid headaches early in the project, I stored Unicode characters
as fixed 4-byte values. The problem with this is that if the editor is
actually used for programming, it's almost impossible to find a non-ASCII
character, meaning some memory would be wasted.
I don't necessarily think it's a problem, but i do plan to change the
storage strategy someday (and consequently, the strategy for access,
removal, inserting, (..........................................)

- The autocomplete algorithm has complexity O(`size of file`).

## Project status

Given the program's simple nature and my goal of creating an editor that is basic yet good enough for my daily use, the `nyt` is technically completely finished, meaning no major changes or features are likely to be added. Future updates will consist mainly of bug fixes, improvements to existing plugins, the addition of new plugins, and possibly some internal optimizations (courageously handling UTF-8 or huge files, for example) that i plan to tackle... who knows when, just for fun (because i don't really think they are needed).

## Contributing

Contributions, bug reports and suggestions are welcome.

Before reporting a bug, check if it has already been discovered
in [`changelog.txt`](./changelog.txt).

## License

This project is licensed under the GNU General Public License v3.0.
See the [LICENSE](LICENSE) file for details.