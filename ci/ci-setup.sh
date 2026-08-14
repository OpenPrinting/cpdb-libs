#!/bin/sh
# ci/ci-setup.sh
#
# CI helper for building and testing cpdb-libs across several CPU
# architectures on both native and QEMU-emulated runners.
#
# cpdb-libs is the D-Bus IPC layer shared by the CPDB frontends and backends
# and MUST NOT depend on CUPS, so there is no CUPS matrix here: the same
# source is exercised only on the four architectures.  All per-leg work lives
# in this script so the workflow legs differ only in whether they run under
# emulation.
#
# Subcommands:
#   deps          install build/test dependencies
#   build         autogen + configure + make + make check
#
# The script runs as root inside emulation containers and via sudo on native
# runners; it detects which automatically.
set -eu

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"

# Make apt completely non-interactive.  Native GitHub runners ship
# needrestart, whose service-restart prompt otherwise hangs the job forever;
# the emulated containers do not have it, which is why only the native legs
# stalled.
export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a
export NEEDRESTART_SUSPEND=1

apt_install() {
	$SUDO apt-get update --fix-missing -y
	$SUDO apt-get install -y "$@"
}

cmd_deps() {
	apt_install \
		build-essential autoconf automake libtool libtool-bin pkg-config \
		gettext autopoint autotools-dev git make gcc \
		libglib2.0-dev libdbus-1-dev dbus
}

cmd_build() {
	./autogen.sh
	./configure
	make -j"$(nproc)" V=1
	# run-tests.sh drives the text frontend inside a private session D-Bus.
	make check V=1 VERBOSE=1 \
		|| { test -f test-suite.log && cat test-suite.log; \
		     cat tools/*.log 2>/dev/null; exit 1; }
}

case "${1:-}" in
	deps)  cmd_deps ;;
	build) cmd_build ;;
	*)
		echo "usage: ci-setup.sh {deps | build}" >&2
		exit 2 ;;
esac
