HOST_BUILD_DIR ?= build-host
HOST_CMAKE_GENERATOR ?= "Unix Makefiles"
HOST_TEST_EXE ?= maishuji-host-tests

DC_BUILD_DIR ?= build-dreamcast
DC_BUILD_TYPE ?= Release
DC_IP ?=
DC_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-smoke.elf
DC_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-smoke.cdi
DC_TEXTURED_ELF ?= $(DC_BUILD_DIR)/maishuji-textured-quad.elf
DC_TEXTURED_CDI ?= $(DC_BUILD_DIR)/maishuji-textured-quad.cdi
DC_PIXEL_SPRITES_ELF ?= $(DC_BUILD_DIR)/maishuji-pixel-sprites.elf
DC_PIXEL_SPRITES_CDI ?= $(DC_BUILD_DIR)/maishuji-pixel-sprites.cdi
DC_LIFECYCLE_ELF ?= $(DC_BUILD_DIR)/maishuji-hello-pvr.elf
DC_LIFECYCLE_CDI ?= $(DC_BUILD_DIR)/maishuji-hello-pvr.cdi
DC_BASIC_3D_ELF ?= $(DC_BUILD_DIR)/maishuji-basic-3d.elf
DC_BASIC_3D_CDI ?= $(DC_BUILD_DIR)/maishuji-basic-3d.cdi
DC_FOGGED_3D_ELF ?= $(DC_BUILD_DIR)/maishuji-fogged-3d.elf
DC_FOGGED_3D_CDI ?= $(DC_BUILD_DIR)/maishuji-fogged-3d.cdi
DC_LIGHTING_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-lighting.elf
DC_LIGHTING_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-lighting.cdi
DC_TEXT_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-text.elf
DC_TEXT_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-text.cdi
DC_PARTICLES_ELF ?= $(DC_BUILD_DIR)/maishuji-particle-batch.elf
DC_PARTICLES_CDI ?= $(DC_BUILD_DIR)/maishuji-particle-batch.cdi
DC_BUDGET_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-budget.elf
DC_BUDGET_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-budget.cdi
DC_ADVANCED_TEXT_ELF ?= $(DC_BUILD_DIR)/maishuji-advanced-multilingual-text.elf
DC_ADVANCED_TEXT_CDI ?= $(DC_BUILD_DIR)/maishuji-advanced-multilingual-text.cdi
DC_PVR_ASSET_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-asset.elf
DC_PVR_ASSET_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-asset.cdi
KOS_MAKEFILE_BUILD_DIR ?= build-kos-makefile-smoke
MKDCDISC ?= mkdcdisc
KOS_TOOLCHAIN_FILE ?= /opt/toolchains/dc/kos/utils/cmake/kallistios.toolchain.cmake

.PHONY: check-toolchain host-configure host-build host-run host-test dreamcast-configure dreamcast-build dreamcast-cdi dreamcast-textured-cdi dreamcast-pixel-sprites-cdi dreamcast-lifecycle-cdi dreamcast-basic-3d-cdi dreamcast-fogged-3d-cdi dreamcast-lighting-cdi dreamcast-text-cdi dreamcast-particles-cdi dreamcast-budget-cdi dreamcast-advanced-text-cdi dreamcast-pvr-asset-cdi dreamcast-makefile-smoke flycast-smoke flycast-textured-quad flycast-pixel-sprites flycast-lifecycle flycast-basic-3d flycast-fogged-3d flycast-lighting flycast-text flycast-particles flycast-budget flycast-advanced-text flycast-pvr-asset run-dc

check-toolchain:
	./tools/with-kos.sh ./tools/check-toolchain.sh

host-configure:
	cmake -S . -B $(HOST_BUILD_DIR) -G $(HOST_CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=Debug

host-build: host-configure
	cmake --build $(HOST_BUILD_DIR) --verbose

host-run: host-build
	./$(HOST_BUILD_DIR)/maishuji-host-smoke

host-test: host-build
	./$(HOST_BUILD_DIR)/$(HOST_TEST_EXE)

dreamcast-configure: check-toolchain
	./tools/with-kos.sh cmake -S . -B $(DC_BUILD_DIR) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$(DC_BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(KOS_TOOLCHAIN_FILE)

dreamcast-build: dreamcast-configure
	./tools/with-kos.sh cmake --build $(DC_BUILD_DIR) --verbose

dreamcast-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_ELF)" -o "$(DC_CDI)" -n "maishuji-lab PVR smoke"

dreamcast-lifecycle-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_LIFECYCLE_ELF)" -o "$(DC_LIFECYCLE_CDI)" -n "maishuji-lab hello PVR lifecycle"

dreamcast-basic-3d-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_BASIC_3D_ELF)" -o "$(DC_BASIC_3D_CDI)" -n "maishuji-lab basic 3D"

dreamcast-lighting-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_LIGHTING_ELF)" -o "$(DC_LIGHTING_CDI)" -n "maishuji-lab PVR offset-color lighting"

dreamcast-text-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_TEXT_ELF)" -o "$(DC_TEXT_CDI)" -n "maishuji-lab PVR text"

dreamcast-particles-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_PARTICLES_ELF)" -o "$(DC_PARTICLES_CDI)" -n "maishuji-lab particle batch"

dreamcast-budget-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_BUDGET_ELF)" -o "$(DC_BUDGET_CDI)" -n "maishuji-lab PVR packet budget"

dreamcast-advanced-text-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_ADVANCED_TEXT_ELF)" -o "$(DC_ADVANCED_TEXT_CDI)" -n "maishuji-lab advanced multilingual text"

dreamcast-pvr-asset-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_PVR_ASSET_ELF)" -o "$(DC_PVR_ASSET_CDI)" -n "maishuji-lab PVR asset"

dreamcast-fogged-3d-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_FOGGED_3D_ELF)" -o "$(DC_FOGGED_3D_CDI)" -n "maishuji-lab fogged 3D"

dreamcast-textured-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_TEXTURED_ELF)" -o "$(DC_TEXTURED_CDI)" -n "maishuji-lab textured quad"

dreamcast-pixel-sprites-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" -e "$(DC_PIXEL_SPRITES_ELF)" -o "$(DC_PIXEL_SPRITES_CDI)" -n "maishuji-lab pixel sprites"

dreamcast-makefile-smoke: check-toolchain
	./tools/with-kos.sh "$(MAKE)" -f tools/kos-makefile-smoke.mk BUILD_DIR="$(KOS_MAKEFILE_BUILD_DIR)"

flycast-smoke:
	./tools/test-flycast-render.sh "$(DC_CDI)"

flycast-lifecycle:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-lifecycle-frame.sh \
	FLYCAST_WINDOW_TITLE=MAISHUJI_HELLO_PVR \
	FLYCAST_STABLE_SAMPLES=1 \
	FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: hello-pvr lifecycle smoke passed" \
	./tools/test-flycast-render.sh "$(DC_LIFECYCLE_CDI)"

flycast-textured-quad:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-textured-frame.sh \
	FLYCAST_WINDOW_TITLE=MAISHUJI_PVR_TEXTURED \
	./tools/test-flycast-render.sh "$(DC_TEXTURED_CDI)"

flycast-pixel-sprites:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-pixel-sprites-frame.sh \
	FLYCAST_WINDOW_TITLE=MAISHUJI_PIXEL_SPRITES \
	FLYCAST_STABLE_SAMPLES=1 \
	./tools/test-flycast-render.sh "$(DC_PIXEL_SPRITES_CDI)"

flycast-basic-3d:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-basic-3d-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_BASIC_3D FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: basic 3D passed (180 frames; projected mesh)" ./tools/test-flycast-render.sh "$(DC_BASIC_3D_CDI)"

flycast-fogged-3d:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-fogged-3d-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_FOGGED_3D FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: fogged 3D passed (180 frames; camera-space linear fog)" ./tools/test-flycast-render.sh "$(DC_FOGGED_3D_CDI)"

flycast-lighting:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-lighting-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_PVR_LIGHTING FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: PVR offset-color lighting passed" ./tools/test-flycast-render.sh "$(DC_LIGHTING_CDI)"

flycast-text:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-text-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_PVR_TEXT FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: PVR text passed" ./tools/test-flycast-render.sh "$(DC_TEXT_CDI)"

flycast-particles:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-particles-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_PARTICLE_BATCH FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: particle batch passed" ./tools/test-flycast-render.sh "$(DC_PARTICLES_CDI)"

flycast-budget:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-budget-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_PVR_BUDGET FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: PVR budget passed" ./tools/test-flycast-render.sh "$(DC_BUDGET_CDI)"

flycast-advanced-text:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-advanced-text-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_ADVANCED_TEXT FLYCAST_STABLE_SAMPLES=1 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: advanced multilingual text passed" ./tools/test-flycast-render.sh "$(DC_ADVANCED_TEXT_CDI)"

flycast-pvr-asset:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-pvr-asset-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_PVR_ASSET FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: PVR asset passed (PVRT ARGB4444; 256x256; translucent alpha)" ./tools/test-flycast-render.sh "$(DC_PVR_ASSET_CDI)"

run-dc: dreamcast-build
	@test -n "$(DC_IP)" || (echo "Set DC_IP to the Dreamcast BBA address, for example: make run-dc DC_IP=YOUR_DREAMCAST_IP" >&2; exit 2)
	./tools/with-kos.sh dc-tool-ip -t "$(DC_IP)" -x "$(DC_ELF)"
