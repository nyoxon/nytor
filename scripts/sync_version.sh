#!/bin/sh

set -eu

mode=${1:-write}
case "$mode" in
    write|--check) ;;
    *)
        echo "usage: $0 [--check]" >&2
        exit 2
        ;;
esac

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
root=$(CDPATH= cd "$script_dir/.." && pwd)
version=$(sed -n 's/^VERSION[[:space:]]*=[[:space:]]*//p' "$root/Makefile")

if ! printf '%s\n' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "invalid or missing VERSION in Makefile: '$version'" >&2
    exit 1
fi

tmp=$(mktemp "${TMPDIR:-/tmp}/nyt-version.XXXXXX")
trap 'rm -f "$tmp"' 0 HUP INT TERM

update_file() {
    file=$1
    kind=$2

    case "$kind" in
        debian)
            awk -v version="$version" '
                /^Version:[[:space:]]/ { $0 = "Version: " version; count++ }
                { print }
                END { if (count != 1) exit 2 }
            ' "$file" > "$tmp"
            ;;
        manual)
            awk -v version="$version" '
                /^\.TH NYTOR 1 / {
                    if (sub(/"nytor [^"]*"/, "\"nytor " version "\"")) count++
                }
                { print }
                END { if (count != 1) exit 2 }
            ' "$file" > "$tmp"
            ;;
        freebsd)
            awk -v version="$version" '
                /^DISTVERSION[[:space:]]*=/ { $0 = "DISTVERSION=\t" version; count++ }
                { print }
                END { if (count != 1) exit 2 }
            ' "$file" > "$tmp"
            ;;
    esac

    if cmp -s "$file" "$tmp"; then
        return
    fi

    if [ "$mode" = "--check" ]; then
        echo "version metadata is out of sync: ${file#"$root"/}" >&2
        return 1
    fi

    chmod 0644 "$tmp"
    mv "$tmp" "$file"
    tmp=$(mktemp "${TMPDIR:-/tmp}/nyt-version.XXXXXX")
    echo "updated ${file#"$root"/} to $version"
}

update_file "$root/manual/nyt.1" manual
update_file "$root/packaging/DEBIAN/control" debian
update_file "$root/packaging/freebsd/nyt/Makefile" freebsd

if [ "$mode" = "--check" ]; then
    echo "version metadata is synchronized at $version"
fi
