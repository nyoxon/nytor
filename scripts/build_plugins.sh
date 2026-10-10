#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$project_root/build/plugins"
shared_dir="$project_root/plugins/shared"

make -C "$project_root" plugins
mkdir -p "$shared_dir"

while IFS= read -r -d '' plugin_file; do
	relative_path="${plugin_file#"$build_dir"/}"
	plugin_name="${relative_path%%/*}"
	cp -- "$plugin_file" "$shared_dir/$plugin_name.so"
done < <(find "$build_dir" -mindepth 3 -maxdepth 3 -type f -name '*.so' -print0)
