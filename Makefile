.DEFAULT_GOAL := help
SANITIZER ?= none
L2CAP_BUILD ?= build/l2cap-$(SANITIZER)
SAN_FLAGS_none :=
SAN_FLAGS_address := -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_FLAGS_thread := -fsanitize=thread -fno-omit-frame-pointer

.PHONY: help test-l2cap
help: ## Show development targets.
	@awk 'BEGIN { print "Usage: make <target> [SANITIZER=none|address|thread] [L2CAP_BUILD=path]\n" } /^[a-zA-Z0-9_.-]+:.*##/ { split($$0, a, ":.*## "); printf "  %-18s %s\n", a[1], a[2] }' $(MAKEFILE_LIST)

test-l2cap: ## Build and run the L2CAP concurrency and error tests.
	@case "$(SANITIZER)" in none|address|thread) ;; *) echo "Unknown SANITIZER: use none, address or thread" >&2; exit 2;; esac
	cmake -S tests/l2cap -B "$(L2CAP_BUILD)" -DCMAKE_CXX_FLAGS="$(SAN_FLAGS_$(SANITIZER))"
	cmake --build "$(L2CAP_BUILD)"
	ctest --test-dir "$(L2CAP_BUILD)" --output-on-failure
