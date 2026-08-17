[ui]

# defines the shell to be used when invoking the 'terminal'
# command within the editor.
# if the value is "auto", the program will attempt to
# use the user's default shell (ie, the value of $SHELL)
# default = "auto"

shell = "auto"


# the color palette that will be used inside the program
# see 'howto' command in order to learn how to create
# your own theme
# default = no theme

theme = "default"


# default tab size used in the program, but the user can
# change temporarily (which means that the changes
# will be discarded when closing the program) the tab size
# of a single file at runtime using the 'set' command
# default = 4

tab_size = 4


# defines whether the editor saves the file automatically
# when quiting the program.
# notice that the program will ignore changes that have
# occured in untitled files.
# this is equivalent to always choose the option "save all, but
# untitled files, and quit" when quiting
# default = false

auto_save_quit = false


# defines whether the tab added by the user will consist
# of spaces or '\t'
# default = false

use_spaces = false


# line_numbers specifies whether the line enumeration of the lines
# of the file should be rendered
# default = true

line_numbers = true


# defines whether a visual representation of the tabs and spaces
# inside a file should be rendered.
# this is the default configuration for the entire file, but the
# user can change it temporarily (which means that the changes
# will be discarded when closing the program) at runtime
# using the ACTION_SHOW_TABS keybind or the 'show' command
# default = false

show_tabs = false


# the cursor style that will be used inside the program. the options
# are: 
# - default (will use the cursor style of the terminal), 
# - steady_bar and blinking_bar, 
# - steady_underline and blinking_underline,
# - steady_block and blinking_block.
# some terminal emulators may ignore the program's attempt to
# change the style of the cursor.
# default = default

cursor_style = blinking_bar


# defines whether the cursor will be constrained by the terminal
# size if it moves out of view while scrolling.
# default = true

cursor_follow_scroll = true


# defines whether the select line operation should
# select the start of the next line
# default = true
select_line_selects_next = true


# defines whether the left button click of the mouse
# might end an active selection.
# the default value is 'false' because there is no
# way to select some text holding the left button
# as you could do in other editors (that is not a limitation
# of any kind, but a internal design decision); for this reason
# i've decided that the right button deletes/creates a selection
# while the left button modifies its boundaries (and therefore,
# does not delete it).
# if you decide that the value should be true, the only easy
# way to select lines, for example, is using the 'select' command/
# keybind + 'goto' command/keybind
# default = false

left_click_end_selection = false


# defines whether the background color should fill all (almost)
# the terminal.
# the default behavior is to fill only the area writable by the
# user, which means that only syntax (bg) colors would be changed and
# the program wouldn't try to paint the horizontal and vertical
# ends of the terminal.
# default = false

background_fills_all = false


# defines whether the inner auto_complete should be used
# default = false
use_autocomplete = false


[keybinds]

# the keybind must be formed by a combination of modifiers
# (ctrl, alt, shift) plus a non-escape ascii character or
# an arrow direction (right, down, left, up).
# there are no support for keys like HOME, INSERT etc or
# F1, F2, FN

# the program will not start if there are ACTIONS with identical
# keybinds

# unfortunately, there are some combinations that have special
# meaning inside the terminal emulator itself and cannot
# be interpreted by nytor correctly given that the emulator
# receive the input first.
# for these cases, an error message will be displayed when
# attempting to open the program

# the default keybinds for each action are:

# save = 				"ctrl+s"
# quit = 				"ctrl+q"
# quit_forced = 		"alt+q"
# find = 				"ctrl+f"
# goto = 				"ctrl+g"
# selection *(see explanation below) = "ctrl+space ('\0' in this case)"
# show_tabs = 			"alt+t"
# move_start_line = 	"alt+w"
# move_end_line = 		"ctrl+d"
# select_line = 		"ctrl+l"
# select_all =			"ctrl+a"
# indent =				"ctrl+p"
# unindent =			"ctrl+o"
# comment =				"ctrl+k"
# copy =				"ctrl+c"
# paste =				"ctrl+v"
# match =				"ctrl+r"
# suspend =				"alt+b"
# terminal =			"ctrl+b"
# cmd =					"ctrl+n"
# replace =				"ctrl+x"
# undo =				"ctrl+z"
# redo =				"ctrl+y"
# del_from_cursor_left	"ctrl+u"
# del_from_cursor_right "alt+u"
# next_file =			"alt+right"
# prev_file =			"alt+lef"
# close_file =			"ctrl+w"
# close_file_forced =	"alt+w"
# new_file =			"ctrl+t"
# selection_move_up = 	"ctrl+shift+up"
# selection_move_down =	"ctrl+shift+down"
# move_word_right = 	"ctrl+shift+right"
# move_word_left =		"ctrl+shift+left"
# move_fullword_right =	"ctrl+alt+right"
# move_fullword_left =	"ctrl+alt+left"
# scroll_right =		"ctrl+right"
# scroll_left =			"ctrl+left"
# scroll_up =			"ctrl+up"
# scroll_down = 		"ctrl+down"
# scroll_terminal_up = 	"alt+up"
# scroll_terminal_down ="alt+down"

# * yes, the program can use escape characteres, but you can't :D
# notice that there isn't either a 'space' key which you can use 
# you can also creates/deletes a selection using the mouse right button

# any error that occurs while reading the keybind will cause the
# default keybind to be used for the corresponding action

# for almost all the action keybinds using letters above, there is
# a corresponding command to be used in the internal cmd.
# there is no corresponding command for any of the action keybinds
# that use arrow directions above.

save = "ctrl+s"
quit = "ctrl+q"
find = "ctrl+f"




[plugins]

# you decide what language plugins should be used in the program
# by additing the following line:
#
# language = lang_name

# an error message will be displayed after starting the program
# if any kind of error while loading a language plugin occured

# unlike other configurations, you can repeat the 'language' option
# to add multiple languages without unwanted behavior. in fact,
# the program will create a list containing all the plugins
# you add:
#
# language = lang_name1
# language = lang_name2

# whenever the editor opens a new file, the program
# will attempt to decide the language plugin to be used based
# on the file extension.
# for this reason, the order in which you place the plugins
# below will be the order the program will use to determine
# whether or not a plugin should be used, stopping
# at the first valid one.

# the note above is important because someone might put
#
# language = c
# language = cpp
#
# to find that the plugin used when opening a .h file is
# the c one, not the cpp one

# you can see what language plugin the editor is using
# for the actual file using the 'show' command

# you can change the language plugin the editor is using
# for the actual file, at runtime, using the 'set' command

# see 'howto' command in order to learn how to create your
# own language plugin

language = c
language = make
language = bash