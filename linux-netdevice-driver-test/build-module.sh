#!/usr/bin/env bash
# Build the dummy-eth / dummy-hw modules against the matching WSL2 kernel.
# No sudo required (run after setup-deps.sh has installed the deps).
set -euo pipefail
#普通的 Ubuntu 系统中，你可以通过安装 linux-headers-$(uname -r) 来直接编译模块。但在 WSL2 中，系统默认不包含 /lib/modules/$(uname -r)/build 目录，也没有现成的头文件包可供安装。因此，你必须手动克隆微软的 WSL2 内核源码，并自己编译一遍来“准备”编译环境。
TAG=linux-msft-wsl-6.18.33.2
SRC="$HOME/wsl-kernel-src"
REPO="/mnt/c/Users/a000707/Documents/linux-driver-test"

# 1. Get the matching kernel source.
if [ ! -d "$SRC/.git" ]; then
    echo ">>> Cloning $TAG ..."
    git clone --depth 1 --branch "$TAG" \
        https://github.com/microsoft/WSL2-Linux-Kernel.git "$SRC"
fi

cd "$SRC"

# 2. Configure exactly like the running kernel.
if [ ! -f .config ]; then
    zcat /proc/config.gz > .config
fi
make olddefconfig

# 3. Prepare the tree and build vmlinux so Module.symvers (with the correct
#    MODVERSIONS CRCs) is generated. This is the slow step.
make -j"$(nproc)" modules_prepare
make -j"$(nproc)" vmlinux

# 4. Build our out-of-tree modules against the prepared tree.
make -j"$(nproc)" -C "$SRC" M="$REPO" modules

echo ">>> Built modules:"
ls -l "$REPO"/*.ko
