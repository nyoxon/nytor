# About

PROGRAM UNDER DEVELOPMENT, once it's finished, i'll add more information here.

this is a simple cli text editor for writing code (primarily C, but with space for other languages) written in C for linux only.

the main objective of this project was to develop a simple CLI text editor, just like vim, neovim etc, but that has really useful keybinds (which the previous editors don't have) like sublime text (which is the editor that i often use)

in my view, the negative sides of the editors like vim are the lack of operability with the mouse and weird mode of uses that transform the use of keybinds kind of unnatural. and the negative side of the subl is its non-free aspect

# Usage (for now)

compile the program:

```bash
make
```

or using gcc sanitize flags:

```bash
make debug
```

then:

```bash
./nytor [ARGUMENTS] [FILEPATHS]
```

to clean object files:

```bash
make clean
```

to run in debug mode:

```bash
./nytor --debug
```

to compile plugins and create shared objects:

```bash
make plugins
```

after that you need to cp/mv the .so file in build/plugins to
~/.config/nytor/plugins/languages/