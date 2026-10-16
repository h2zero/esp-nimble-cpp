# L2CAP host tests

These tests compile the production `NimBLEL2CAPChannel.cpp` with small replacements for NimBLE and FreeRTOS. Separate host, callback and writer threads exercise send errors, buffer ownership, early credit notifications, disconnects, whole-message serialization, timeout cleanup, reconnect and callback queue overflow.

```sh
make test-l2cap
make test-l2cap SANITIZER=address
make test-l2cap SANITIZER=thread
```

Clang, CMake and a C++17 compiler are required. Address builds also enable undefined-behavior checks. The deferred-callback configuration and an inline-callback build are both compiled.

The fake radio does not validate ESP32 scheduling, RF behavior, controller credits or memory-pool sizing. Real-device transfer and disconnect tests remain necessary. Timeouts deliberately exercise the production two-second deadline. The lifecycle cases repeatedly create and destroy channels and verify that the shared callback task and queue are released after the last channel.

## Callback and ownership contract

Whole writes are serialized and a failed incomplete write closes the channel.
Inline onConnect/onRead callbacks remain on the host by default. They must not
block; use an application worker for writes that may need credits. The optional
read-offload mode moves those callbacks to a shared worker. Disconnect callbacks
always use that worker and follow running deferred reads. Stop accepting, request
disconnect and wait for `isIdle()` before deleting an application-owned channel;
never delete it inside one of its callbacks. The library owns/deletes callback
objects passed to the constructor/service. Creating/destroying channels belongs
to one application initialization/management task.

This change allocates full-SDU mbufs (five by default) instead of subdividing
SDUs into 250-byte pool blocks. Pool count is configurable; larger MTUs/counts
cost more per-channel heap. The shared worker is released after the last channel.

CI compiles a CoC-enabled probe with inline and deferred callbacks on IDF 5.4.3
and 5.5.1. These are compile/link checks; real radio tests remain separate.

`.github/tests/l2cap-build/main/main.cpp` shows bounded, worker-driven echo.
Callbacks queue copied bytes, overflow disconnects, and a generation counter
rejects old queued work after reconnect. Its service/channel live for the process
lifetime; a product with teardown must also stop/drain its application worker
before deleting the channel. The CI probe does not advertise or perform RF tests.
