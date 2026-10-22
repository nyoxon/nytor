#!/bin/sh

set -eu

if [ "$(uname -s)" != FreeBSD ]; then
    echo "FreeBSD packages must be built on FreeBSD" >&2
    exit 1
fi

command -v gmake >/dev/null 2>&1 || {
    echo "gmake is required (pkg install gmake)" >&2
    exit 1
}
command -v pkg >/dev/null 2>&1 || {
    echo "pkg is required to create the package" >&2
    exit 1
}

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
root=$(CDPATH= cd "$script_dir/.." && pwd)
version=$(sed -n 's/^VERSION[[:space:]]*=[[:space:]]*//p' "$root/Makefile")
abi=$(pkg config ABI)
work=$(mktemp -d "${TMPDIR:-/tmp}/nyt-package.XXXXXX")
stage=$work/stage
source=$work/source
metadata=$work/metadata
output=${PKG_OUTPUT_DIR:-$root/dist}

trap 'rm -rf "$work"' 0 HUP INT TERM

mkdir -p "$source" "$stage" "$metadata"
mkdir -p "$output"
output=$(CDPATH= cd "$output" && pwd)

# Build an isolated source copy so this target does not leave root-owned build
# artifacts or staged files in the user's checkout.
cp -R "$root/Makefile" "$root/config" "$root/include" \
    "$root/manual" "$root/plugins" "$root/src" "$root/themes" "$source/"

gmake -C "$source" CC=cc PREFIX=/usr/local DESTDIR="$stage" install

cat > "$metadata/+MANIFEST" <<EOF
{
  "name": "nyt",
  "version": "$version",
  "origin": "editors/nyt",
  "comment": "Terminal-based text editor designed for programming",
  "maintainer": "arturo.teles.ao@gmail.com",
  "www": "https://github.com/nyoxon/nytor",
  "prefix": "/usr/local",
  "abi": "$abi",
  "deps": {},
  "categories": ["editors"],
  "licenses": ["GPLv3+"],
  "licenselogic": "single"
}
EOF

cat > "$metadata/+DESC" <<'EOF'
Nytor is a terminal-based text editor written in C and designed for programming.
This package includes its default configuration, themes, Lua scripts, and
compiled language lexer plugins.
EOF

plist=$work/pkg-plist
{
    echo '@owner root'
    echo '@group wheel'
    (cd "$stage/usr/local" && find bin share -type f -print)
} > "$plist"

pkg create -m "$metadata" -p "$plist" -r "$stage" -o "$output" -v
echo "Package created under $output"
