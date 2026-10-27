### DATE: dd/mm/yy

### version: 1.3.0



## 25/10/26

### BUGFIX: 
some insertion operations were still possible on
readonly files, which would cause the program to crash
(don't ask me how). In particular:

- add an open delimiter for a language
- autocomplete

the editor would crash with a floating-point exception;
by debuggin with GDB, i discovered that this was occuring
in `stack_at`, more specifcally, within the module operation
contained inside that function. This makes sense, since
in readonly mdoe the undo/redo stacks should be empty,
and therefore the statement corresponding to the
module operation would attempt to use 0 as the right-most
operand (which is not mathematically defined).

## 26/10/26

### CHANGE: 
while using the editor in a VT on FreeBSD, i noticed
that the program would flicker when i scrolled rapidly.
I discovered that this was happening because of calling
`clean_terminal` and keeping the terminal's cursor
visible during the render.
To keep things more efficent, i had to perform the same
operation that would have occured if `has_background`
were present: drawing to the end of the line and down
to the last line. For this reason, the background_fills_all
setting has been removed.
Therefore, the rendering is more efficient, though
not highly efficient, because it was decided from
the very start of the project (for weird reason) to
use system calls (such as `write`) directly; this
has become a burden to carry in the editor's current state,
since the buffering used by stdio
functions has proven useful.

### CHANGE: 
motivated by the previous change, there now exists
`prompt_deactivate` that clears the prompt's memory,
sets active = 0 and is responsible for clearing
the visual representation of the prompt from the screen

### BUGFIX: 
when changing the theme while the program was
running, some colors ended up looking bizarre.
This happened because the editor didn't clear the colors
present in the configuration before loading the new theme,
which led it to used the old colors.

### CHANGE/BUGFIX: 
the editor didn't synchronize the `normal`
colors between the UI and themes, which led to some
inconsistencies when switching themes. For this reason,
the `normal` settings has been removed from themes and
is now strictly synchronized with the UI's `normal`.

### WARNING:
regarding the previous changes, i haven't performed a
deeper analysis, so there might be hiddern bugs or new
ones introduced as a result of modifying these specific areas


## 27/10/26

### CHANGE:
the `is_word_char` function in assembly lexer now
only considers letters, numbers, and underscores