# L2CAP send regression tests

The tests compile the production `NimBLEL2CAPChannel.cpp` with minimal native
host replacements. They check positive native error propagation, abort after a
partly submitted SDU without replay, caller/native mbuf ownership, append
failure cleanup, successful fragmentation and accepted credit-stalled sends.
They do not simulate RF, controller buffers or concurrency.

```sh
cmake -S tests/l2cap -B build/l2cap-tests
cmake --build build/l2cap-tests
ctest --test-dir build/l2cap-tests --output-on-failure
```
