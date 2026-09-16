<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: Copyright (c) 2026 N-iX -->

# DALI Engineering Measurements

This document applies to the `feature/cpu-load-measurement` engineering branch.
The diagnostics, workloads, and results described here are not part of the
public sample or a product certification claim.

## Scope

The measurements use an nRF54L15 DK with the CPUAPP controller sample and the
CPUFLPR DALI runtime. The optional BLE workload starts non-connectable
advertising at a 20 ms interval before DALI initialization. It is repeatable
radio activity, not a connected Bluetooth Mesh, Thread, or Matter workload.

The DALI TX timing measurements are captured at CPUFLPR GPIO P1.11 with DK
GND. They characterize the Nordic controller output before the external DALI
PHY. They do not characterize the electrical waveform on the DALI bus.

## CPU Load Results

CPU load reporting measures total non-idle time for each core. It includes
interrupt handling and does not attribute time to individual threads.

| Measurement | No BLE workload | BLE advertising workload |
| --- | ---: | ---: |
| CPUAPP `network_setup` | 0.5% | 1.1% |
| CPUAPP periodic load | about 0.1% | 0.7-1.1% |
| CPUFLPR periodic load | 0.0% | 0.0-0.4% |

`network_setup` spans discovery and the initial actual-level queries. CPUFLPR
reports 100% for individual DALI operation windows because it is non-idle for
the duration of that operation; it is not sustained CPU utilization.

## CPUFLPR TX Timing Results

The analysis excludes the inter-frame start-condition interval, not a valid
Manchester half-bit. The half-bit nominal value is 416.667 us.

| Metric | Baseline, 186 packets | BLE advertising, 228 packets |
| --- | ---: | ---: |
| Half-bit mean error | +6.346 us | +6.342 us |
| Packet jitter p-p median | 0.80 us | 0.80 us |
| Packet jitter p-p mean | 1.19 us | 2.08 us |
| Packet jitter p-p maximum | 37.4 us | 297.6 us |

The mean half-bit value and typical jitter did not shift under the BLE
workload. The data contains isolated extra-edge outliers in both captures.
They are not sufficient to establish a BLE or EMI causal relationship and are
not used as a claim about DALI bus conformance. Repeated captures are required
before publishing an outlier rate.

## IPC Round-Trip Measurement

Build all diagnostics together:

```sh
source env.sh
west build -p always -d build-ipc-latency-ble \
  -b nrf54l15dk/nrf54l15/cpuapp samples/dali_controller \
  -- -DDALI_CPU_LOAD_MEASUREMENT=ON \
     -DDALI_BLE_LOAD=ON \
     -DDALI_IPC_LATENCY_MEASUREMENT=ON
```

`DALI_IPC_LATENCY_MEASUREMENT` uses two active-high board LEDs as markers:

- LED2 rises immediately before CPUAPP sends the request and falls in the
  CPUAPP response callback.
- LED3 rises when CPUFLPR receives the request and falls immediately before
  CPUFLPR sends the response.

For every real DALI API call, the following edge differences describe the
request path:

- LED2 rising to LED3 rising: CPUAPP-to-CPUFLPR request delivery.
- LED3 high time: FLPR work scheduling and DALI operation execution.
- LED3 falling to LED2 falling: CPUFLPR-to-CPUAPP response delivery.
- LED2 high time: complete DALI API request-to-response duration.

Connect two logic-analyzer channels to LED2, LED3, and DK GND. The marker
signals include a small, stable GPIO-operation overhead at each edge.

Preliminary captures of real `dali_api_set_level_short()` calls measured
20-30 us from the LED2 rising edge to the LED3 rising edge. This is the
application-visible CPUAPP-to-CPUFLPR request path, including the two GPIO
marker operations, IPC transport, and CPUFLPR callback scheduling; it is not
a raw hardware-peripheral latency claim. The associated DALI TX frame started
about 200 us after the LED2 rising edge on average. The latter includes the
request path and FLPR scheduling before transmission begins.
