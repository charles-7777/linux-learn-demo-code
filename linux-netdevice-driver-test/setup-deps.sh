#!/usr/bin/env bash
# Installs the packages needed to build the matching WSL2 kernel + our modules.
# Run this once:  sudo bash setup-deps.sh
set -e
apt-get update
apt-get install -y \
    build-essential \
    flex bison \
    libssl-dev libelf-dev \
    dwarves bc kmod cpio \
    libncurses-dev pahole \
    git
echo "setup-deps: done"
