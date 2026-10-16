import io
import json
import copy
from contextlib import redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest

import bench


class EvidenceTests(unittest.TestCase):
    def test_reboot_during_long_run_fails_without_waiting_for_full_command_budget(self):
        class Serial:
            def write(self, data): pass
            def read(self, count): return b'BENCH {"event":"ready"}\n'
        console = bench.Console.__new__(bench.Console)
        console.serial = Serial(); console.log = io.BytesIO()
        console.buffer = bytearray(); console.events = []
        with self.assertRaisesRegex(RuntimeError, "restarted"):
            console.command("run duplex 4096 4096 4096", timeout=3600)

    def test_tx_diagnostics_preserve_credit_and_stall_pairs(self):
        result = bench.tx_stats(b"I (120) BLETx: sdus=32 bytes=40000 credits_min/max=0/100 stalls=2 errors=0 queue_us_avg/max=4/10 send_us_avg/max=6/12 stall_us_total/max=100/70 (host submission, not RF)")
        self.assertEqual(result["credits_min/max"], [0, 100])
        self.assertEqual(result["stall_us_total/max"], [100, 70])
        self.assertEqual(result["log_timestamp_ms"], 120)
        self.assertEqual(result["event"], "txstats")
        self.assertIsNone(bench.tx_stats(b"I (120) other log line"))
    def test_stream_requires_both_receipts_and_complete_sample_sequences(self):
        receipt = {"token": 42, "frames": 2, "bytes": 128, "errors": 0, "first_us": 100, "last_us": 200}
        result = {"event": "run-result", "success": True, "completed": 2, "token": 42,
                  "tx_frames": 2, "rx_frames": 2, "callback_faults": 0, "peer_finish_confirmed": True,
                  "local_rx": copy.deepcopy(receipt), "peer_rx": copy.deepcopy(receipt)}
        events = [result] + [{"event": "stream-tx-sample", "seq": i, "tx_submit_us": 1} for i in range(2)]
        events += [{"event": "stream-rx-sample", "seq": i, "received_at_us": 100 + 100 * i} for i in range(2)]
        bench.checked_measurement(events, 2, "duplex", 64, 64)
        bad = copy.deepcopy(events); bad.pop()
        with self.assertRaisesRegex(RuntimeError, "incomplete"):
            bench.checked_measurement(bad, 2, "duplex", 64, 64)
        for summary, key, value in (("peer_rx", "bytes", 127), ("peer_rx", "token", 41),
                                    ("local_rx", "errors", 1), ("local_rx", "last_us", 201)):
            bad = copy.deepcopy(events); bad[0][summary][key] = value
            with self.assertRaises(RuntimeError):
                bench.checked_measurement(bad, 2, "duplex", 64, 64)
        bad = copy.deepcopy(events); bad[0]["peer_finish_confirmed"] = False
        with self.assertRaises(RuntimeError):
            bench.checked_measurement(bad, 2, "duplex", 64, 64)

    def test_lifecycle_excludes_initial_allocations_but_retains_a_leak_trend(self):
        cycles = [{"disconnected_heap": {"free": 10000 - i * 64, "internal_free": 10000 - i * 64,
                                         "largest_internal_block": 4000, "tasks": 7}} for i in range(10)]
        result = bench.lifecycle_summary(cycles)
        self.assertEqual(result["warmup_cycles_excluded"], 5)
        self.assertEqual(result["free"]["slope_per_cycle"], -64)
        self.assertEqual(result["tasks"]["slope_per_cycle"], 0)

    def test_empty_measurement_cannot_be_reported_as_success(self):
        for field, value in (("count", 0), ("repeat", 0), ("warmup", -1), ("cycles", 0)):
            arguments = dict(sizes="64", count=64, repeat=3, warmup=8, request_size=8)
            arguments[field] = value
            with self.assertRaises(ValueError):
                bench.run(SimpleNamespace(**arguments))

    def test_comparison_keeps_distinct_link_parameters_separate(self):
        with TemporaryDirectory() as directory:
            paths = []
            for index, (interval, mtu, rtt) in enumerate([(15, 1251, 100), (30, 1251, 200),
                                                         (15, 2048, 300), (15, 1251, 120)]):
                path = Path(directory) / f"run-{index}.json"
                path.write_text(json.dumps({"success": True, "dut_gap_role": "peripheral", "runs": [{
                    "pattern": "echo", "request_size": 64, "response_size": 64,
                    "params": [{"interval_ms": interval, "mtu": mtu, "tx_phy": 2, "rx_phy": 2,
                                "tx_octets": 251, "rx_octets": 251}],
                    "result": {"success": True, "rtt_median_us": rtt, "rtt_p95_us": rtt,
                               "goodput_bytes_per_s": 1000}}]}))
                paths.append(str(path))
            output = io.StringIO()
            with redirect_stdout(output):
                bench.compare(SimpleNamespace(files=paths))
            changes = [line.split("|")[-2].strip() for line in output.getvalue().splitlines()[2:]]
            self.assertEqual(changes, ["+0.0%", "+0.0%", "+0.0%", "+20.0%"])

    def test_missing_or_duplicate_sample_cannot_pass(self):
        result = {"event": "run-result", "success": True, "completed": 3}
        for sequences in ([0, 2], [0, 1, 1], [0, 2, 1]):
            events = [result] + [{"event": "sample", "seq": n} for n in sequences]
            with self.assertRaisesRegex(RuntimeError, "incomplete"):
                bench.checked_run(events, 3)

    def test_success_requires_requested_count(self):
        events = [{"event": "run-result", "success": True, "completed": 1},
                  {"event": "sample", "seq": 0}]
        with self.assertRaisesRegex(RuntimeError, "unexpected sample count"):
            bench.checked_run(events, 3)

    def test_serial_chunks_can_split_json_at_any_byte(self):
        events = [{"event": "ready"}, {"event": "command-end", "code": 0}]
        wire = b"boot log\n" + b"".join(b"BENCH " + json.dumps(e).encode() + b"\n" for e in events)

        class Serial:
            def __init__(self):
                self.chunks = iter(bytes([byte]) for byte in wire)
                self.sent = []

            def read(self, count):
                return next(self.chunks, b"")

            def write(self, data):
                self.sent.append(data)

        console = bench.Console.__new__(bench.Console)
        console.serial = Serial()
        console.log = io.BytesIO()
        console.buffer = bytearray()
        console.events = []
        self.assertEqual(console.command("status"), events)
        self.assertEqual(console.log.getvalue(), wire)
        self.assertEqual(console.serial.sent, [b"status\n"])


if __name__ == "__main__":
    unittest.main()
