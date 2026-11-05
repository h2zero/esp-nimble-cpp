#!/usr/bin/env python3
"""Drive the serial L2CAP bench. Raw samples are saved alongside the full log."""
import argparse
import datetime as dt
import json
from pathlib import Path
import statistics
import re
import sys
import time


def tx_stats(line):
    match = re.search(rb"BLETx:\s+(.*)", line)
    if not match:
        return None
    fields = {key.decode(): [int(v) for v in value.split(b"/")] if b"/" in value else int(value)
              for key, value in re.findall(rb"(\w+(?:/\w+)*)=([0-9/]+)", match[1])}
    if not fields:
        return None
    fields["event"] = "txstats"
    stamp = re.search(rb"\(([0-9]+)\)", line)
    if stamp:
        fields["log_timestamp_ms"] = int(stamp[1])
    return fields


class Console:
    def __init__(self, port, log):
        import serial
        self.serial = serial.Serial(port, 115200, timeout=0.1)
        self.log = log
        self.buffer = bytearray()
        self.events = []

    def command(self, text, timeout=20):
        self.serial.write((text + "\n").encode())
        deadline = time.monotonic() + timeout
        received = []
        while time.monotonic() < deadline:
            data = self.serial.read(8192)
            if not data:
                continue
            self.log.write(data)
            self.log.flush()
            self.buffer.extend(data)
            while b"\n" in self.buffer:
                line, _, rest = self.buffer.partition(b"\n")
                self.buffer = bytearray(rest)
                marker = line.find(b"BENCH ")
                if marker < 0:
                    stats = tx_stats(line)
                    if stats:
                        self.events.append(stats); received.append(stats)
                    continue
                event = json.loads(line[marker + 6:])
                self.events.append(event)
                received.append(event)
                if event["event"] == "ready" and text.split()[0] not in ("status", "reboot"):
                    raise RuntimeError(f"device restarted during {text!r}")
                if event["event"] == "error":
                    print("DUT:", event["message"], file=sys.stderr)
                if event["event"] == "command-end":
                    if event["code"] != 0:
                        raise RuntimeError(f"{text!r} failed with code {event['code']}")
                    return received
        raise TimeoutError(f"No command-end for {text!r} after {timeout}s")

    def close(self):
        self.serial.close()


def save(path, document):
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(document, indent=2) + "\n")
    temporary.replace(path)


def checked_run(events, requested_count):
    result = next(event for event in events if event["event"] == "run-result")
    samples = [event for event in events if event["event"] == "sample"]
    if [s["seq"] for s in samples] != list(range(result["completed"])):
        raise RuntimeError("serial evidence is incomplete: missing or reordered samples")
    if result["success"] and result["completed"] != requested_count:
        raise RuntimeError("successful run reported an unexpected sample count")
    return result, samples


def params_match(params, args):
    return (params.get("interval_ms") == args.interval and
            params.get("tx_phy") == args.phy and params.get("rx_phy") == args.phy)


def checked_measurement(events, count, pattern, request_size, response_size):
    if pattern in ("echo", "request"):
        return checked_run(events, count)
    result = next(event for event in events if event["event"] == "run-result")
    expected_tx = count if pattern != "stream-rx" else 0
    expected_rx = count if pattern != "stream-tx" else 0
    samples = [event for event in events if event["event"] in ("stream-tx-sample", "stream-rx-sample")]
    for direction, expected, size, summary_name in (("tx", expected_tx, request_size, "peer_rx"),
                                                     ("rx", expected_rx, response_size, "local_rx")):
        directional = [s for s in samples if s["event"] == f"stream-{direction}-sample"]
        if [s["seq"] for s in directional] != list(range(expected)):
            raise RuntimeError(f"incomplete stream {direction} sample evidence")
        if result[f"{direction}_frames"] != expected:
            raise RuntimeError(f"stream {direction} count mismatch")
        if expected:
            summary = result[summary_name]
            if (summary["token"] != result["token"] or summary["frames"] != expected or
                    summary["bytes"] != expected * size or summary["errors"] != 0 or
                    summary["first_us"] > summary["last_us"]):
                raise RuntimeError(f"invalid stream {direction} receipt")
            if direction == "rx":
                arrivals = [s["received_at_us"] for s in directional]
                if (arrivals != sorted(arrivals) or arrivals[0] != summary["first_us"] or
                        arrivals[-1] != summary["last_us"]):
                    raise RuntimeError("inconsistent stream arrival evidence")
    if (not result["success"] or result["completed"] != count or
            not result["peer_finish_confirmed"] or result["callback_faults"]):
        raise RuntimeError("stream did not complete on both peers")
    return result, samples


def lifecycle_summary(cycles):
    settled = cycles[min(5, max(0, len(cycles) - 2)):]
    heaps = [c["disconnected_heap"] for c in settled]
    result = {"completed_cycles": len(cycles), "warmup_cycles_excluded": len(cycles) - len(settled)}
    for key in ("free", "internal_free", "largest_internal_block", "tasks"):
        values = [h[key] for h in heaps]
        result[key] = {"first": values[0], "last": values[-1], "min": min(values), "max": max(values)}
        if len(values) > 1:
            result[key]["slope_per_cycle"] = statistics.linear_regression(range(len(values)), values).slope
    return result


def run(args):
    sizes = [int(value) for value in args.sizes.split(",")]
    if (not 1 <= args.count <= 4096 or not 0 <= args.warmup <= 4096 or args.repeat < 1 or getattr(args, "cycles", 1) < 1 or
            not sizes or any(not 12 <= size <= 4996 for size in sizes) or
            not 8 <= args.request_size <= 4996):
        raise ValueError("count: 1..4096, warmup: 0..4096, repeat: >=1, sizes: 12..4996, request-size: 8..4996")
    now = dt.datetime.now(dt.timezone.utc)
    path = (Path(args.output) if args.output else
            Path(__file__).resolve().parent / "results" / f"{now:%Y%m%dT%H%M%SZ}-{args.label}.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    document = {"schema": 2, "created_at": now.isoformat(), "label": args.label,
                "peer_name": args.peer, "dut_gap_role": args.dut_role,
                "notes": args.notes, "runs": [], "success": False}
    dut = peer = None
    with path.with_suffix(".log").open("wb") as log:
        try:
            if args.peer_port:
                peer_log = path.with_suffix(".peer.log").open("wb")
                peer = Console(args.peer_port, peer_log)
                peer_status = peer.command("status")
                if any(e.get("tx_diagnostics") for e in peer_status if e["event"] == "hello"):
                    raise ValueError("ESP reflector TX diagnostics need a continuous peer-console reader; enable diagnostics on the DUT only")
                peer.command("role server")
            dut = Console(args.port, log)
            initial = dut.command("status")
            document["initial_status"] = initial
            document["cycles"] = []
            for cycle in range(getattr(args, "cycles", 1)):
                document["cycle_in_progress"] = cycle
                if args.peer_port and cycle:
                    peer.command("role server")
                if args.dut_role == "central":
                    dut.command("role client")
                dut.command(f"params {args.interval} {args.phy}")
                if args.dut_role == "peripheral":
                    dut.command("listen")
                    connected_events = dut.command("await 30", timeout=40)
                else:
                    connected_events = dut.command(f"connect {args.peer}", timeout=40)
                negotiated = next(e for e in reversed(connected_events)
                                  if e["event"] == "params" and e.get("connected"))
                document["negotiated"] = negotiated
                document.setdefault("parameters_match", True)
                document["parameters_match"] &= params_match(negotiated, args)
                print(f"Connected cycle {cycle + 1}: DUT {args.dut_role}, interval {negotiated.get('interval_ms')} ms, "
                      f"PHY {negotiated.get('tx_phy')}/{negotiated.get('rx_phy')}, "
                      f"DLE {negotiated.get('tx_octets')}/{negotiated.get('rx_octets')}, "
                      f"MTU {negotiated['mtu']}", flush=True)
                if not params_match(negotiated, args):
                    print("Peer did not accept the requested interval/PHY", file=sys.stderr)
                    if args.require_params:
                        raise RuntimeError("negotiated parameters do not match the requested A/B setup")
                for repeat in range(args.repeat):
                    for size in sizes:
                        streaming = args.pattern in ("stream-tx", "stream-rx", "duplex")
                        request = size if args.pattern == "echo" or streaming else args.request_size
                        warmup = []
                        if args.warmup:
                            warmup = dut.command(f"run {args.pattern} {request} {size} {args.warmup}",
                                                 timeout=args.warmup * 6 + 30)
                            if not checked_measurement(warmup, args.warmup, args.pattern, request, size)[0]["success"]:
                                raise RuntimeError("warmup failed")
                        events = dut.command(f"run {args.pattern} {request} {size} {args.count}",
                                             timeout=args.count * 6 + 30)
                        result, samples = checked_measurement(events, args.count, args.pattern, request, size)
                        run_params = [e for e in events if e["event"] == "params" and e.get("connected")]
                        if args.require_params and len(run_params) < 2:
                            raise RuntimeError("missing link parameters before/after the measured run")
                        if any(not params_match(e, args) for e in run_params):
                            document["parameters_match"] = False
                            if args.require_params:
                                raise RuntimeError("link parameters changed during the comparison")
                        document["runs"].append({"cycle": cycle, "repeat": repeat, "pattern": args.pattern,
                                                 "request_size": request, "response_size": size,
                                                 "warmup": warmup, "params": run_params,
                                                 "result": result, "samples": samples,
                                                 "txstats": [e for e in events if e["event"] == "txstats"]})
                        document["events"] = dut.events
                        save(path, document)
                        timing = (f"TX {result['tx_frames']}, RX {result['rx_frames']}, " if streaming else
                                  f"median {result['rtt_median_us'] / 1000:.3f} ms, p95 {result['rtt_p95_us'] / 1000:.3f} ms, ")
                        print(f"{args.pattern} {request}->{size}, cycle {cycle + 1}, repeat {repeat + 1}: " + timing +
                              f"{result.get('goodput_bytes_per_s', 0) / 1024:.2f} KiB/s, "
                              f"ok={result['success']}", flush=True)
                        if not result["success"]:
                            raise RuntimeError("benchmark reported a failed run")
                disconnected = dut.command("disconnect", timeout=10)
                heap = next(e for e in reversed(disconnected) if e["event"] == "heap")
                document["cycles"].append({"cycle": cycle, "negotiated": negotiated, "disconnected_heap": heap})
                document["events"] = dut.events
                save(path, document)
            document.pop("cycle_in_progress", None)
            document["lifecycle"] = lifecycle_summary(document["cycles"])
            document["success"] = True
        except Exception as error:
            document["error"] = str(error)
            raise
        finally:
            if dut:
                try:
                    if "cycle_in_progress" in document or not document.get("cycles"):
                        dut.command("disconnect", timeout=10)
                except Exception as error:
                    document["disconnect_error"] = str(error)
                    document["success"] = False
                document["events"] = dut.events
                dut.close()
            if peer:
                try:
                    peer.command("role client", timeout=10)
                except Exception as error:
                    document["peer_disconnect_error"] = str(error)
                    document["success"] = False
                document["peer_events"] = peer.events
                peer.close()
                peer_log.close()
            save(path, document)
            print(f"Saved {path}", flush=True)
    if document.get("disconnect_error") or document.get("peer_disconnect_error"):
        raise RuntimeError(document.get("disconnect_error") or document["peer_disconnect_error"])


def compare(args):
    print("| File | DUT GAP role | Interval ms | PHY TX/RX | MTU | Pattern | Request → response | Median RTT ms | p95 ms | Goodput KiB/s | Change basis | Change |")
    print("|---|---|---:|---|---:|---|---:|---:|---:|---:|---|---:|")
    baseline = {}
    for path in map(Path, args.files):
        document = json.loads(path.read_text())
        if not document["success"]:
            raise ValueError(f"{path}: incomplete or failed measurement file")
        grouped = {}
        role = document.get("dut_gap_role", "central")
        fallback_params = document.get("negotiated") or next(
            (e for e in document.get("events", []) if e["event"] == "params" and e.get("connected")), {})
        for run in document["runs"]:
            recorded_params = run.get("params") or [fallback_params]
            params = recorded_params[0]
            link_fields = ("interval_ms", "tx_phy", "rx_phy", "mtu", "tx_octets", "rx_octets")
            if any(tuple(p.get(k) for k in link_fields) != tuple(params.get(k) for k in link_fields)
                   for p in recorded_params[1:]):
                raise ValueError(f"{path}: link parameters changed within a run")
            key = (run["pattern"], run["request_size"], run["response_size"],
                   params.get("interval_ms"), params.get("tx_phy"), params.get("rx_phy"),
                   params.get("mtu"), params.get("tx_octets"), params.get("rx_octets"))
            if not run["result"]["success"]:
                raise ValueError(f"{path}: failed run cannot be compared as a successful baseline")
            grouped.setdefault(key, []).append(run["result"])
        for key, results in grouped.items():
            timed = "rtt_median_us" in results[0]
            median = statistics.median(r["rtt_median_us"] for r in results) if timed else None
            p95 = statistics.median(r["rtt_p95_us"] for r in results) if timed else None
            goodput = statistics.median(r["goodput_bytes_per_s"] for r in results)
            metric = median if timed else goodput
            reference = baseline.setdefault((role,) + key, metric)
            change = 100 * (metric / reference - 1) if reference else 0
            median_text = f"{median / 1000:.3f}" if timed else "—"
            p95_text = f"{p95 / 1000:.3f}" if timed else "—"
            basis = "RTT" if timed else "goodput"
            print(f"| {path.name} | {role} | {key[3]} | {key[4]}/{key[5]} | {key[6]} | {key[0]} | {key[1]} → {key[2]} | {median_text} | "
                  f"{p95_text} | {goodput / 1024:.2f} | {basis} | {change:+.1f}% |")



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    measure = commands.add_parser("run")
    measure.add_argument("--port", required=True)
    measure.add_argument("--peer", default="auto", help="Advertised name, BLE address, or auto (exactly one benchmark service)")
    measure.add_argument("--peer-port", help="Also configure an ESP reflector over this serial port")
    measure.add_argument("--dut-role", choices=["central", "peripheral"], default="central",
                         help="Peripheral reproduces Apple-Central/ESP-Peripheral product roles")
    measure.add_argument("--pattern", choices=["echo", "request", "stream-tx", "stream-rx", "duplex"], default="echo")
    measure.add_argument("--sizes", default="64,256,1024,4096")
    measure.add_argument("--request-size", type=int, default=8)
    measure.add_argument("--cycles", type=int, default=1, help="Connect/run/disconnect cycles; heap is captured after each teardown")
    measure.add_argument("--count", type=int, default=64)
    measure.add_argument("--warmup", type=int, default=8)
    measure.add_argument("--repeat", type=int, default=3)
    measure.add_argument("--interval", type=float, default=15)
    measure.add_argument("--phy", type=int, choices=[1, 2], default=2)
    measure.add_argument("--require-params", action="store_true",
                         help="Fail if the peer does not accept the requested interval/PHY (for A/B tests)")
    measure.add_argument("--label", default="baseline")
    measure.add_argument("--notes", default="")
    measure.add_argument("--output")
    measure.set_defaults(func=run)
    comparison = commands.add_parser("compare")
    comparison.add_argument("files", nargs="+")
    comparison.set_defaults(func=compare)
    args = parser.parse_args()
    if args.command == "run" and args.peer_port and args.dut_role != "central":
        parser.error("--peer-port currently requires --dut-role central")
    try:
        args.func(args)
    except Exception as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
