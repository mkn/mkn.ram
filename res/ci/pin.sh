#!/usr/bin/env bash
# Dependency versions for a release, see .github/workflows/release.yml
#
#   pin.sh resolve   prints "<key> <tag>" for each mkn.yaml property <dep>.version: latest, <tag> being
#                    the newest x.y.z tag of github.com/mkn/<dep>; modules (github.com/mkn-mod/<dep>)
#                    keep their latest branch
#   pin.sh apply     sets those properties in mkn.yaml, read from $PINS (resolve's output)
set -euo pipefail

case "${1:-}" in
resolve)
  for key in $(yq '.property // {} | to_entries | .[] | select(.key == "*.version" and .value == "latest") | .key' mkn.yaml); do
    dep=${key%.version}
    # modules (github.com/mkn-mod) have no releases: their latest branch, left as is
    if git ls-remote "https://github.com/mkn-mod/$dep" > /dev/null 2>&1; then
      echo "$key: a module, stays latest" >&2; continue
    fi
    tags=$(git ls-remote --tags --refs --sort=-v:refname "https://github.com/mkn/$dep") || tags=
    pin=$(sed -n 's#.*refs/tags/\([0-9]*\.[0-9]*\.[0-9]*\)$#\1#p' <<< "$tags" | head -1)
    if [ -z "$pin" ]; then echo "no release tag found for $dep" >&2; exit 1; fi
    echo "$key $pin"
  done
  ;;
apply)
  # sed and grep only (BSD sed on macOS, no yq there): changes only that line, yq -i would reformat it
  while read -r key pin; do
    [ -n "$key" ] || continue
    k=${key//./\\.}
    sed -i.bak "s/^\( *$k: *\)latest/\1$pin/" mkn.yaml && rm mkn.yaml.bak
    grep -q "^ *$k: *$pin\( \|$\)" mkn.yaml || { echo "could not pin $key" >&2; exit 1; }
  done <<< "${PINS:-}"
  ;;
*)
  sed -n '2,8s/^# \{0,1\}//p' "$0"
  exit 1
  ;;
esac
