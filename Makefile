HOST_BUILD_DIR ?= build-host
HOST_CMAKE_GENERATOR ?= "Unix Makefiles"

DC_BUILD_DIR ?= build-dreamcast
DC_BUILD_TYPE ?= Release
DC_IP ?=
DC_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-smoke.elf
DC_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-smoke.cdi
MKDCDISC ?= mkdcdisc
KOS_TOOLCHAIN_FILE ?= /opt/toolchains/dc/kos/utils/cmake/kallistios.toolchain.cmake

.PHONY: check-toolchain host-configure host-build host-run dreamcast-configure dreamcast-build dreamcast-cdi run-dc

check-toolchain:
	./tools/with-kos.sh ./tools/check-toolchain.sh

host-configure:
	cmake -S . -B $(HOST_BUILD_DIR) -G $(HOST_CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=Debug

host-build: host-configure
	cmake --build $(HOST_BUILD_DIR) --verbose

host-run: host-build
	./$(HOST_BUILD_DIR)/maishuji-host-smoke

dreamcast-configure: check-toolchain
	./tools/with-kos.sh cmake -S . -B $(DC_BUILD_DIR) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$(DC_BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(KOS_TOOLCHAIN_FILE)

dreamcast-build: dreamcast-configure
	./tools/with-kos.sh cmake --build $(DC_BUILD_DIR) --verbose

dreamcast-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_ELF)" -o "$(DC_CDI)" -n "maishuji-lab PVR smoke"

run-dc: dreamcast-build
	@test -n "$(DC_IP)" || (echo "Set DC_IP to the Dreamcast BBA address, for example: make run-dc DC_IP=YOUR_DREAMCAST_IP" >&2; exit 2)
	./tools/with-kos.sh dc-tool-ip -t "$(DC_IP)" -x "$(DC_ELF)"
