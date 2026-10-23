### Some things i plan to add

- [X] Increase the number of functions available in
the Lua API

Ex: remove_line, add_line etc. It's possible to remove or
add a line to the file by using a sequence of existing
functions, but it would be more practical to have a single
API function that handles it.

- [X] Write some useful scripts

The current `longest` and `trim` scripts are actually
quite useful, but the idea is to write similar or even
better scripts. In any case, any operation on text
within the file could simply be performed by more 
specialized programs like `sed` and `awk`; you
just need to synchronize the file afterward.

- [X] Please improve the code's security; some critical
functions even don't propagate errors for fuck's sake.
I'd be a good ideia to revise some parts of the code as well.

- [ ] Enable the editor to read from standard input via
a pipe.