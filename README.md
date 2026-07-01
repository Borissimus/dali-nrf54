# dali-nrf54

`dali-nrf54` is an external nRF Connect SDK / Zephyr add-on module for DALI-2
control on the nRF54L15 DK.

The current implementation targets:

- `nrf54l15dk/nrf54l15/cpuapp`
- `nrf54l15dk/nrf54l15/cpuflpr`

## Architecture

- CPUAPP / Cortex-M33 hosts the Zephyr frontend driver and public DALI API.
- CPUFLPR / RISC-V hosts the low-level DALI runtime and all DALI frame
  handling.
- IPC uses shared memory plus IRQ-driven signaling.
- The public control surface is exposed as `dali_api_*` over IPC.

## Current Scope

The module currently provides:

- a public `dali_api_*` surface on CPUAPP,
- an IPC bridge between CPUAPP and CPUFLPR,
- a prebuilt FLPR-side DALI engine library,
- the `samples/dali_controller` end-to-end demo.

## Current Demo Behavior

`samples/dali_controller` is the current end-to-end demo path.

At startup the demo:

- initializes the CPUAPP frontend and FLPR IPC runtime,
- runs DALI discovery and assigns short addresses,
- caches discovered short addresses on CPUAPP.

If discovery returns one or more devices, the demo uses addressed group mode
based on short-address parity:

- `sw0`: toggle all devices with even short addresses
- `sw1`: toggle all devices with odd short addresses
- `sw2`: step the even-address group brightness by `+20%`, cycling
  `100% -> 0%`
- `sw3`: step the odd-address group brightness by `+20%`, cycling
  `100% -> 0%`

If discovery fails or returns no devices, the demo falls back to broadcast:

- `sw0`: turn all devices off
- `sw1`: turn all devices on at max level
- `sw2`: decrease all devices by `20%`
- `sw3`: increase all devices by `20%`

CPUAPP button handling includes software debounce.

## Layout

- `include/dali/`: public API and shared types.
- `drivers/dali/`: CPUAPP-side frontend and IPC transport.
- `zephyr/blobs/`: fetched prebuilt FLPR DALI engine archives.
- `cpuflpr/src/`: open FLPR runtime shell and IPC-facing integration.
- `samples/dali_controller/`: CPUAPP controller sample and sysbuild entry point.
- `zephyr/module.yml`: Zephyr module integration metadata.
- `west.yml`: workspace manifest for bootstrapping from this repository.

## Host Requirements

Install the base system packages first:

```sh
sudo apt update
sudo apt install -y \
  git \
  cmake \
  ninja-build \
  gperf \
  ccache \
  dfu-util \
  device-tree-compiler \
  wget \
  curl \
  xz-utils \
  file \
  make \
  gcc \
  g++ \
  python3 \
  python3-venv \
  python3-pip
```

Install Zephyr SDK `0.16.8`:

```sh
cd ~/Downloads
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64.tar.xz
tar -xf zephyr-sdk-0.16.8_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh
```

## Set Up A Workspace

Choose any directory where the NCS workspace should live. The commands below use
`dali_ws` only as an example.

```sh
mkdir dali_ws
cd dali_ws
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip west

west init -m git@github.com:Borissimus/dali-nrf54.git .
west update
west blobs fetch dali-nrf54
```

Install the Python requirements used by Zephyr, NCS, and MCUboot:

```sh
python -m pip install \
  -r zephyr/scripts/requirements.txt \
  -r nrf/scripts/requirements.txt \
  -r bootloader/mcuboot/scripts/requirements.txt
```

If the repository has already been cloned manually into the workspace, initialize
west from the local checkout instead:

```sh
west init -l dali-nrf54
west update
west blobs fetch dali-nrf54
```

## Build CPUAPP + CPUFLPR

Build the CPUAPP sample. Sysbuild automatically builds the CPUFLPR runtime as a
secondary image and wires the IPC link between CPUAPP and CPUFLPR.

```sh
west build -p always \
  -b nrf54l15dk/nrf54l15/cpuapp \
  dali-nrf54/samples/dali_controller
```

The sysbuild output used for flashing is:

```sh
build/merged.hex
```

Flash from the top-level build directory:

```sh
west flash -d build --runner nrfutil
```

Expected startup flow:

- CPUAPP starts `dali_controller`
- CPUFLPR starts the IPC server
- discovery runs on FLPR
- CPUAPP logs the discovered device count

## Pre-review Checks

Run the same local validation bundle that is used by CI:

```sh
./dali-nrf54/scripts/pre_review.sh
```

The script performs:

- `west blobs fetch dali-nrf54`,
- a pristine sysbuild of `samples/dali_controller`,
- a `twister` build-only run for the sample,
- `checkpatch` on `HEAD~1..HEAD`,
- `check_compliance` on `HEAD~1..HEAD` with `KconfigBasicNoModules`
  excluded for the NCS workspace case.

Generated reports and intermediate artifacts are stored under:

```sh
dali-nrf54/build-pre-review/
```

## CI

GitHub Actions uses the same validation flow as local development.

The workflow:

1. checks out the repository with full history,
2. sets up Python,
3. uses the official Zephyr setup action to bootstrap the workspace and
   toolchains,
4. runs `./scripts/pre_review.sh`,
5. verifies that no tracked files were modified by the checks.

If CI fails, the workflow uploads the key reports from
`build-pre-review/` as artifacts.

### Test CI Locally

The practical local check is to run the same script that CI runs:

```sh
./dali-nrf54/scripts/pre_review.sh
```

This gives the same build, `twister`, `checkpatch`, and compliance behavior as
the workflow itself. The GitHub Actions workflow only bootstraps the workspace
and then delegates to the script.

## Build CPUFLPR Only

For low-level FLPR development, the CPUFLPR application can also be built on its
own:

```sh
west build -p always \
  -b nrf54l15dk/nrf54l15/cpuflpr \
  dali-nrf54/cpuflpr
```

This standalone build is intended for FLPR-side iteration. Depending on the NCS
board support, sysbuild may still add the platform CPUAPP launcher image, but it
does not build the `samples/dali_controller` CPUAPP sample.

If the board qualifier differs in the active NCS checkout, inspect available
boards with:

```sh
west boards | grep -i nrf54l15
```
