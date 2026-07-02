#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026 Borys Nykytiuk

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workspace_dir="$(dirname "$repo_dir")"

source "$workspace_dir/.venv/bin/activate"
export ZEPHYR_SDK_INSTALL_DIR=/home/borys/toolchains/zephyr-sdk-1.0.1
