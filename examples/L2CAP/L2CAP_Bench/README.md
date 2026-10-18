# L2CAP CoC benchmark

Standalone ESP-IDF 6.1 firmware for ESP32-S3 and ESP32-S31, with `echo`,
small-request/large-response `request`, `stream-tx`, `stream-rx` and `duplex`.
It can be the DUT or the ESP reflector. Wi-Fi/Ethernet are not initialized.
The ESP drives the application protocol independently of its GAP role.

## Build and connect

```sh
# Use the matching ESP-IDF 6.1 environment.
idf.py --version
IDF_TARGET=esp32s3 idf.py -B build-s3 -D SDKCONFIG=build-s3/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32s3' build
idf.py -B build-s3 -p /dev/serial/by-id/<board> flash
```

For S31 use `IDF_TARGET=esp32s31`, `idf.py --preview`, `build-s31` and
`sdkconfig.defaults.esp32s31`. CPU defaults are 240/320 MHz respectively.
The USB Serial/JTAG console accepts LF-terminated commands at 115200 baud.

An ESP peer uses the same firmware and `role server`. For macOS use the
[`l2cap-reflect` companion](https://github.com/mickeyl/core-bluetooth-tool)
and its `tools/run-l2cap-reflector-app.sh`; macOS must grant Bluetooth access.
The companion shares Foundation-only framing/codecs for a future iPhone peer;
an iPhone peer is not included here. Start the Mac with
`--connect L2CAP-Bench-ESP` for Apple-Central/ESP-Peripheral roles.

```text
params 15 2
listen
await 30
run echo 64 64 64
run request 8 4096 64
run duplex 4096 4096 128
disconnect
```

For ESP-Central mode use `connect auto` instead of `listen`/`await`. Automatic
selection requires exactly one benchmark service; a name or BLE address can be
selected explicitly. `role client` restores DUT mode after using the ESP as a
reflector. A running reflector has a separate application TX worker.

The Python runner automates this over USB; see
[`tools/l2cap-bench`](../../../tools/l2cap-bench/README.md). Use
`--require-params` for controlled comparisons. Link setup retries a refused
interval after one second, up to three times, outside callbacks and timing.
The peer still chooses the accepted parameters: requested values are not proof.

## Wire contract, version 2

Frames are `B7 type length:u16 payload`, with at most 4996 payload bytes.
Multibyte fields are big endian except the GATT PSM's little-endian `u16`.
The parser accepts split and coalesced frames; SDUs are not message boundaries.
The service UUID is `e7100001-61b3-4b3d-8b4d-7e3b51e2c001`; its PSM read
characteristic is `e7100002-61b3-4b3d-8b4d-7e3b51e2c001`.

| Type | Payload |
|---|---|
| `01 HELLO` / `81 reply` | Version `02` followed by UTF-8 identity. |
| `02 CONFIG` / `82 reply` | Pattern `u8`, request/response sizes `u16`, count `u32`; reply status `00` means accepted. Patterns: 1 echo, 2 request, 3 stream-tx, 4 stream-rx, 5 duplex. |
| `03 PING` / `83 reply` | Sequence/token `u32`, then deterministic data. Reply adds reflector turnaround `u32` microseconds after token. |
| `06 START` / `86 READY` | Run token `u32`; receive state is armed before READY. |
| `04 DATA` | Sequence/token `u32` followed by deterministic data. |
| `05 END` | Token/count `u32`. |
| `85 RECEIPT` | Token/count `u32`, received bytes/first timestamp/last timestamp `u64`, errors `u32` (36 bytes). |
| `07 FINISH` / `87 finished` | Token `u32`; confirmation requires all enabled directions and receipts complete. |
| `7F ABORT` | Optional UTF-8 reason. |

CLI sizes include message metadata and exclude the four-byte frame header.
Echo/response/stream sizes are 12..4996 bytes; an asymmetric request may use 8. CONFIG count is 1..4096. Data byte `i` is
`(sequence + token + i) & 255`, starting after sequence/token (and turnaround in
a reply). Streaming has no per-DATA application ACK. A failed/incomplete write
or validation aborts the run. There is a benchmark-only 10-second receive
no-progress watchdog; this is not an ECU diagnostic timeout.

## Metrics and interpretation

`hello` identifies Git/IDF bases, SDK-config and ELF hashes, CPU, buffer counts
and options. Per-run parameters include actual interval, PHY, MTU and observed
DLE. Records use blocking USB writes after timing; all timed per-frame DUT
samples are retained in memory and emitted afterwards. Reflector writes never
run in a native receive callback. Fast streaming validation runs in that
callback without console output or writes.

Echo/request RTT ends at complete reply parsing on the DUT clock. The first
callback timestamp means delivery of a complete SDU, not first RF byte. Peer
turnaround is measured on its own clock and its subtraction still leaves a
round trip. `tx_submit_us` measures blocking host submission, not remote use.

Stream wall goodput divides useful pattern bytes in all enabled directions by
START-through-FINISH duration, including control round trips. Arrival goodput
uses each receiver's own first/last timestamps, excludes the first frame, and
never compares clocks across peers. Neither metric is physical BLE throughput.

Reconnect cycles close CoC/GAP, allow 100 ms for RTOS cleanup and report total/
internal free heap, largest internal block and tasks. The runner excludes five
initial cycles from heap trends. Parser capacity may retain a bounded high-water
allocation; compare settled cycles within one build/role. Short runs do not
prove day-long leak freedom. Failed files remain failed evidence.

## Known resource limit

With very large peer credits, unpaced large SDUs can exhaust native HCI/MSYS
buffers. CoC credit availability does not bound local controller backlog.
The library aborts the incomplete write instead of replaying a partial SDU.
This benchmark deliberately reports that failure; it does not silently pace or
retry the run. Experimental resource-window, alignment and native diagnostic
changes are outside this submission. Use echo/request as a bounded-load
baseline and preserve failed streaming captures when investigating the limit.

The default pool profile also failed an ESP-S3→ESP-S31 single-request echo at
4996 payload bytes plus framing (one 5000-byte SDU, negotiated MTU 5000). An
error-only instrumented check returned code 6 (ENOMEM) at the first write. That
identifies a resource failure, not the exact limiting pool; it is retained as a
failed case and is not claimed fixed here. Use 4096-byte payloads for the bounded
baseline. The parser's maximum frame size is not a guarantee that the default
host/controller buffers can sustain every transfer geometry.
