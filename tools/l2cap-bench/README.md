# Serial runner

Python 3 and pyserial are required. The ESP-IDF Python environment includes
pyserial. Start the macOS reflector or an ESP in `role server` before running.

For the ECOS GAP roles, launch the Mac app with
`bash tools/run-l2cap-reflector-app.sh --connect L2CAP-Bench-ESP` from the
core-bluetooth-tool repository, then use `--dut-role peripheral` below. The
ESP still initiates and times all benchmark messages; Apple opens GAP and CoC.

```sh
python bench.py run --port /dev/serial/by-id/<S3> \
  --peer auto --pattern echo --sizes 64,256,1024,4096 \
  --count 64 --warmup 8 --repeat 3 --label mac-s3-baseline
python bench.py run --port /dev/serial/by-id/<S3> \
  --pattern request --request-size 8 --sizes 32,256,1024,4096 \
  --count 64 --repeat 3 --label mac-s3-request
python bench.py compare results/*baseline.json results/*candidate.json
```

```sh
python bench.py run --port /dev/serial/by-id/<S3> --dut-role peripheral \
  --pattern request --request-size 8 --sizes 32,256,1024,4096 \
  --count 64 --warmup 8 --repeat 3 --label apple-central-s3
```

Sizes and request/response direction are always relative to the ESP DUT:
`request --request-size 8 --sizes 4096` loads Apple→ESP; use
`request --request-size 4096 --sizes 32` to load ESP→Apple. These are raw
transport patterns; the separate ECOS benchmark reproduces product requests.
Use `--require-params` for A/B runs. Without it, refused interval/PHY requests
remain measurable, with a warning and the actual values stored in the file.

For ESP↔ESP, use `--peer L2CAP-Bench-ESP --peer-port <second-board>`. The runner
sets that board to reflector mode. Keep board placement fixed and record distance,
orientation, Mac/iPhone model and any concurrent radio use in `--notes`.

Default output is `tools/l2cap-bench/results/` regardless of the working
directory; this directory is Git-ignored. `--output` selects another path.

The benchmark firmware requires ESP-IDF 6.1. Each file contains firmware identity, negotiated parameters, all per-packet
samples, warmup events and the aggregate results. The full serial stream stays
beside it as `.log`; an optional reflector console has `.peer.log`. Results are
saved after each measured size, including partial failures. `compare` refuses
failed runs and reports the median of repeated run medians/p95 values.

The four-byte frame header is excluded from `--sizes`; sequence/token and
turnaround are included. See the firmware README for the exact wire layout and
timing definitions. RTT changes compare with the first matching file supplied
to `compare` within the same GAP role. Warmup measurements are retained but
excluded from its table. The role column prevents silently mixing inverse and
product-role baselines. Missing/reordered samples fail the run, including warmup.

Streaming and reconnect examples (matching version-2 firmware/peer):

```sh
python bench.py run --port /dev/serial/by-id/<S3> --dut-role peripheral \
  --pattern duplex --sizes 64,4096 --count 128 --warmup 8 --repeat 3 \
  --require-params --label duplex
python bench.py run --port /dev/serial/by-id/<S31> --dut-role peripheral \
  --pattern duplex --sizes 64,4096 --count 4 --warmup 0 --repeat 1 \
  --cycles 100 --require-params --label reconnect
```

`stream-tx` sends ESP→peer, `stream-rx` sends peer→ESP, `duplex` sends both.
For these patterns the CLI uses each `--sizes` value for both configured sizes.
Streaming has no RTT median: comparison tables use dashes and explicitly mark
changes as goodput changes. Goodput is the sum of enabled directions and includes
the START/FINISH round trips; inspect per-receiver arrival rates in the JSON too.

Every enabled direction needs a complete per-frame sample sequence, matching
receipt, zero errors and a confirmed FINISH. Missing evidence fails the run.
Schema 2 adds `cycles`, per-run cycle numbers, heap trends in `lifecycle` and
parsed `txstats`; old echo/request files remain readable by `compare`.
The CLI fails on a broken cycle instead of hiding it with an automatic retry.
Teardown still runs and partial evidence is saved. Failed files cannot be used
as successful comparison baselines.

Optional TX-stat records from separately instrumented firmware are parsed when
present. This example does not enable internal instrumentation or tuning.
