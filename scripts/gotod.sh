#!/bin/bash

# I'm not sure if i'll create a plugin that performs the same
# function as this script. In any case, this script can be useful
# for finding the definition or declaration of something in a
# given language

# Usage: script <language> [name]

# name is optional, if it's not passed, all definitions and declar
# will be printed at once

# Example: script c foo
# Output might be:
	# {
	#   "name": "foo",
	#   "kind": "prototype",
	#   "path": "main.c",
	#   "line": 1
	# }
	# {
	#   "name": "foo",
	#   "kind": "function",
	#   "path": "main.c",
	#   "line": 7
	# }


print_usage() {
	printf "Usage: $0 <language> [name]\n\n" >&2
	printf "language (mandatory): language to be considered\n"
	printf "name (optional): name of a specific symbol\n\n"
	printf "If name is omitted, all the defs and dcls will be\n"
	printf "printed at once.\n"
}


if (( $# == 0 )) || (( $# > 2 )); then
	print_usage
	exit 1;
fi


# Gets the string in the exact format that `ctags` accepts
lang=$(ctags --list-languages | awk -v language="$1" '
	BEGIN { IGNORECASE = 1  }
	$0 == language { print; exit  }
')

if [[ -z "$lang" ]]; then
	echo "unknown language: $1" >&2
	exit 1
fi

name=$2


# I don't know if this is the standard for everyone, but my
# map for C came without the .h extension, whereas the one
# for C++ came with every existing extension
# If this is the standard, congrats to the devs, it's truly sublime :D
# This section forces ctags to consider .h files
if [[ "$lang" = "C" ]]; then
	ctags -R \
		--langmap=C:+.h \
		--output-format=json \
		--fields=+n \
		--languages=C \
		--kinds-C='*' \
		. | \
	jq --arg name "$name" '
		select($name == "" or .name == $name) | 
		{ 
			name, 
			kind, 
			path, 
			line  
		}
	'
else
	ctags -R \
		--output-format=json \
		--fields=+n \
		--languages="$lang" \
		--kinds-${lang}='*' \
		. | \
	jq --arg name "$name" '
		select($name == "" or .name == $name) |
		{
			name,
			kind,
			path,
			line
		}
	'
fi

exit $?