# About

this is a simple cli text editor for writing code (primarily C, but with space for other languages) written in C for linux only and for learning purposes.

the main objective of this project was to develop a simple CLI text editor, just like vim, neovim etc, but that has really useful keybinds (which the previous editors don't have) like sublime text (which is the editor that i often use)

in my view, the negative sides of the editors like vim are the lack of operability with the mouse and weird mode of uses that transform the use of keybinds kind of unnatural. and the negative side of the subl is its non-free aspect

it's possible that in the future i'll transfer the content from this editor to and actual application, where you can use the mouse properly

i haven't added a undo/redo system and i haven't modularized/documented the code in a dignified way because i'm too lazy right now, but maybe i'll do it in the distant future

# Program structure

the program consist of the following files:

src/

main.c -> controls the main flow of the program


editor/

- editor.* -> functions to handle key operations within the editor
- file.* -> functions to handle key operations within files
- cursor.* -> functions to handle cursor operations
- scroll.* -> functions to handle scroll (relative and global cursor positions)
- selection.* -> functions to handle selections (text selections)


terminal/

- input.* -> functions to handle input
- syntax_high.* -> functions to aux handling syntax high (the main operation is located on src/editor/editor.c)

util/

- buffer.* -> functions and definitions that has to do with buffers
- debug.* -> functions to handle debug
- error.* -> functions and definitions to handle error manipulation (maybe useless)
- vector.* -> a vector definition and implementation

# Usage

compile the program:

```bash
make
```

then:

```bash
build/nytor [FILENAME] [FLAGS]
```

the FILENAME MUST be passed and FLAGS are:

- --debug (create a file "debug.ny" in which some debug message are written)
- --no-lines (does not render the number of rows in the editor)

to clean object files:

```bash
make clean
```

## Possible operations and keybinds

the possible operations inside the program are:

- save the file (CTRL + s)
- exit the program (CTRL + q)
- goto a specific line (CTRL + g, then write the desired line)
  
  special behaviors:
  - 0 -> EOF
  - 1 -> start of the file
  - number greater than the file size -> EOF

- search of a specific pattern (CTRL + f or CTRL + r)
- show tabs (CTRL + t)
- move cursor to the beginning of the line (CTRL + w)
- move cursor to the beginning of the indentation of the line (CTRL + e)
- move cursor to the end of the line (CTRL + d)
- start/end a selection (CTRL + SPACE)
- select all file (CTRL + a)
- select the actual line (CTRL + l)
- copy the actual selection (CTRL + c)
- paste the actual copyboard selection (CTRL + v)
- create a new line (ENTER)
- indent a line or selection (TAB or CTRL + p)
- unindent a line or selection (CTRL + o)
- (dis)comment a line or selection (CTRL + k)
- scroll the file up (CTRL + ARROW_UP)
- scroll the file down (CTRL + ARROW_DOWN)
- scroll the file up a TERMINAL_SIZE (ALT + ARROW_UP)
- scroll the file down a TERMINAL_SIZE (ALT + ARROW_DOWN)
- move a line or selection up (CTRL + SHIFT + ARROW_UP)
- move a line or selection down (CTRL + SHIFT + ARROW_DOWN)
- move between words (CTRL + SHIFT + ARROW_RIGHT/ARROW_LEFT)
- delete a char, selection or line (BACKSPACE)
- insert a char (just write something)
- move the cursor (use the arrows)

## How to change keybinds

you can change a default keybind by changing the specific keybind in src/editor/editor.c. if the desired keybind is not any of the ones listed above, you need to add it to include/terminal/input.h

# Some implementation details

- the program clipboard is internal to the program and is not shared with your system clipboard
- a file is read and its lines are stored one by one within a vector. it is not the optimal way to treat the problem, but it serves
- the file is completely rewritten every time it is changed
- the internal difference between CTRL + f and CTRL + r are the following: the former looks for all the matches and stores them in a vector, the latter looks only for the next pattern without storing the previous ones anywhere
- there is still no very strict treatment with cases where the size of the line exceeds the horizontal size of the terminal, so be careful :D
- maybe the error.* files are completely useless
- the program might crash if it receives an external shutdown signal
- i haven't tested many cases, so maybe there are several bugs
