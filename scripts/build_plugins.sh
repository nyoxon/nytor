#!/bin/sh
set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd "$script_dir/.." && pwd)
build_dir="$project_root/build/plugins/lexers"
shared_dir="$project_root/plugins/shared"

if [ -n "${MAKE:-}" ]; then
	make_cmd=$MAKE
elif [ "$(uname -s)" = FreeBSD ]; then
	make_cmd=gmake
else
	make_cmd=make
fi

"$make_cmd" -C "$project_root" plugins
mkdir -p "$shared_dir"

find "$build_dir" -type f -name '*.so' -exec sh -c '
	build_dir=$1
	shared_dir=$2
	plugin_file=$3
	relative_path=${plugin_file#"$build_dir"/}
	plugin_name=${relative_path%%/*}
	cp "$plugin_file" "$shared_dir/$plugin_name.so"
' sh "$build_dir" "$shared_dir" {} \;
