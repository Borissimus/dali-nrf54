#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026 Borys Nykytiuk
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-pre-review}"
BOARD="${BOARD:-nrf54l15dk/nrf54l15/cpuapp}"
SAMPLE_DIR="${SAMPLE_DIR:-${ROOT_DIR}/samples/dali_controller}"
TWISTER_OUT="${TWISTER_OUT:-${BUILD_DIR}/twister-out}"
BASE_REV="${BASE_REV:-HEAD~1}"

log() {
    printf '\n==> %s\n' "$1"
}

fail() {
    printf '\nERROR: %s\n' "$1" >&2
    exit 1
}

FAILED_STEPS=()
COMPLIANCE_OUT="${BUILD_DIR}/compliance.xml"

move_new_compliance_artifacts() {
    local before_file="$1"
    local after_file
    local artifact

    after_file="$(mktemp)"
    find "${ROOT_DIR}" -maxdepth 1 -type f -name '*.txt' \
        -printf '%f\n' | sort > "${after_file}"

    while IFS= read -r artifact; do
        [[ -n "${artifact}" ]] || continue
        mv "${ROOT_DIR}/${artifact}" "${BUILD_DIR}/${artifact}"
    done < <(comm -13 "${before_file}" "${after_file}")

    rm -f "${after_file}"
}

run_step() {
    local name="$1"
    shift

    log "${name}"
    if ! "$@"; then
        FAILED_STEPS+=("${name}")
    fi
}

find_zephyr_base() {
    if [[ -n "${ZEPHYR_BASE:-}" && -d "${ZEPHYR_BASE}" ]]; then
        printf '%s\n' "${ZEPHYR_BASE}"
        return 0
    fi

    if command -v west >/dev/null 2>&1; then
        local west_top
        west_top="$(west topdir 2>/dev/null || true)"
        if [[ -n "${west_top}" && -d "${west_top}/zephyr" ]]; then
            printf '%s\n' "${west_top}/zephyr"
            return 0
        fi
    fi

    fail "$(printf '%s' \
        'Unable to locate ZEPHYR_BASE. Export ZEPHYR_BASE or run inside a ' \
        'west workspace.')"
}

ZEPHYR_BASE_DIR="$(find_zephyr_base)"
CHECKPATCH="${ZEPHYR_BASE_DIR}/scripts/checkpatch.pl"
COMPLIANCE="${ZEPHYR_BASE_DIR}/scripts/ci/check_compliance.py"

[[ -f "${CHECKPATCH}" ]] || fail "Missing checkpatch.pl at ${CHECKPATCH}"
[[ -f "${COMPLIANCE}" ]] || fail "Missing check_compliance.py at ${COMPLIANCE}"

cd "${ROOT_DIR}"
mkdir -p "${BUILD_DIR}"

log "Environment"
printf 'ROOT_DIR=%s\n' "${ROOT_DIR}"
printf 'ZEPHYR_BASE=%s\n' "${ZEPHYR_BASE_DIR}"
printf 'BOARD=%s\n' "${BOARD}"
printf 'BASE_REV=%s\n' "${BASE_REV}"

run_step "Fetch module blobs" \
    west blobs fetch dali-nrf54

run_step "Build sample" \
    west build -p always -d "${BUILD_DIR}" -b "${BOARD}" "${SAMPLE_DIR}"

run_step "Run Twister" \
    west twister -T "${SAMPLE_DIR}" -p "${BOARD}" --outdir "${TWISTER_OUT}"

run_step "Run checkpatch" \
    perl "${CHECKPATCH}" --git "${BASE_REV}..HEAD"

run_compliance_step() {
    local before_file
    local rc=0

    before_file="$(mktemp)"
    find "${ROOT_DIR}" -maxdepth 1 -type f -name '*.txt' \
        -printf '%f\n' | sort > "${before_file}"

    python3 "${COMPLIANCE}" \
        -e KconfigBasicNoModules \
        -e SysbuildKconfigBasicNoModules \
        -c "${BASE_REV}..HEAD" \
        -o "${COMPLIANCE_OUT}" || rc=$?

    move_new_compliance_artifacts "${before_file}"
    rm -f "${before_file}"

    return "${rc}"
}

run_step "Run compliance" run_compliance_step

if (( ${#FAILED_STEPS[@]} > 0 )); then
    printf '\nPre-review checks failed:\n' >&2
    for step in "${FAILED_STEPS[@]}"; do
        printf '  - %s\n' "${step}" >&2
    done
    exit 1
fi

log "Pre-review checks completed"
