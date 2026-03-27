CXX ?= g++

HOST_BIN := build/host/chzzk_host
COMMON_SRCS := \
	src/common/chzzk_client.cpp \
	src/common/https_http_client.cpp \
	src/common/m3u8.cpp
HOST_SRCS := src/host/main.cpp

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic -Iinclude -Ithird_party
HOST_LDFLAGS := -lssl -lcrypto

.PHONY: all host run-fixture run-network switch clean vendor

all: host

vendor:
	./scripts/fetch_vendor_headers.sh

$(HOST_BIN): vendor $(COMMON_SRCS) $(HOST_SRCS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(COMMON_SRCS) $(HOST_SRCS) -o $@ $(HOST_LDFLAGS)

host: $(HOST_BIN)

run-fixture: $(HOST_BIN)
	./$(HOST_BIN) --fixture

run-network: $(HOST_BIN)
	./$(HOST_BIN) --network

switch:
	@if [ -z "$$DEVKITPRO" ]; then \
		echo "DEVKITPRO is not set. Install devkitPro first, then run 'make -f Makefile.switch'."; \
		exit 1; \
	fi
	$(MAKE) -f Makefile.switch

clean:
	rm -rf build third_party
