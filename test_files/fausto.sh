#!/bin/bash

filename="fausto.pdf"
pages="doc_pages"

if [[ $# -eq 1 ]]; then
	if [[ "$1" == "clear" ]]; then
		> $pages
	fi

	exit 1;
elif [ $# -eq  2 ]; then
	if [[ "$1" == "set" && ("$2" =~ ^[0-9]+$)  ]]; then
		if grep -q "^$filename " $pages; then
			sed -i "s/^$filename .*/$filename $2/" $pages
		else
			echo "$filename $2" >> $pages
		fi
	fi

	exit 1;
fi

page=$(awk -v name="$filename" '$1 == name {print $2}' $pages)

mupdf $filename $page