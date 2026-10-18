#!/bin/bash -e
# ci-setup.sh: in the riscos-crossdev container image on GitHub, install
# (as root) the host packages riscos-gtk's build and tests use that the
# image doesn't have yet (riscos-crossdev 1.8.1 lacks meson, python3-pil
# and shared-mime-info, among others: build/check-host.sh). Does nothing
# when they're all there, and only reports them when not run as root.
# MIT licence.
cd "$(dirname "$0")/.."
missing=$(build/check-host.sh --missing check)
[ -n "$missing" ] || exit 0
if [ "$(id -u)" != 0 ]; then
  echo "ci-setup.sh: missing: $missing (not root, so not installed)" >&2
  exit 0
fi
echo "ci-setup.sh: installing $missing"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends $missing > /dev/null
build/check-host.sh check
