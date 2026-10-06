HOST_BUILD_DIR ?= build-host
HOST_CMAKE_GENERATOR ?= "Unix Makefiles"
HOST_TEST_EXE ?= maishuji-host-tests

DC_BUILD_DIR ?= build-dreamcast
DC_BUILD_TYPE ?= Release
DC_IP ?=
# Default lesson image used by `run-dc`; override DC_ELF to run another lesson.
DC_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-smoke.elf
# Default self-booting image produced by `dreamcast-smoke-cdi`.
# Override DC_CDI when packaging a different ELF.
DC_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-smoke.cdi
DC_LIFECYCLE_ELF ?= $(DC_BUILD_DIR)/maishuji-hello-pvr.elf
DC_LIFECYCLE_CDI ?= $(DC_BUILD_DIR)/maishuji-hello-pvr.cdi
DC_TEXTURED_ELF ?= $(DC_BUILD_DIR)/maishuji-textured-quad.elf
DC_TEXTURED_CDI ?= $(DC_BUILD_DIR)/maishuji-textured-quad.cdi
DC_PIXEL_SPRITES_ELF ?= $(DC_BUILD_DIR)/maishuji-pixel-sprites.elf
DC_PIXEL_SPRITES_CDI ?= $(DC_BUILD_DIR)/maishuji-pixel-sprites.cdi
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
DC_TEXTURED_3D_ELF ?= $(DC_BUILD_DIR)/maishuji-textured-3d.elf
DC_TEXTURED_3D_CDI ?= $(DC_BUILD_DIR)/maishuji-textured-3d.cdi
DC_HEIGHTMAPPED_TERRAIN_ELF ?= $(DC_BUILD_DIR)/maishuji-heightmapped-terrain.elf
DC_HEIGHTMAPPED_TERRAIN_CDI ?= $(DC_BUILD_DIR)/maishuji-heightmapped-terrain.cdi
DC_TERRAIN_WALK_ELF ?= $(DC_BUILD_DIR)/maishuji-terrain-walk.elf
DC_TERRAIN_WALK_CDI ?= $(DC_BUILD_DIR)/maishuji-terrain-walk.cdi
DC_MIPMAP_TEXTURE_ELF ?= $(DC_BUILD_DIR)/maishuji-mipmap-texture.elf
DC_MIPMAP_TEXTURE_CDI ?= $(DC_BUILD_DIR)/maishuji-mipmap-texture.cdi
DC_MODEL_LOADER_ELF ?= $(DC_BUILD_DIR)/maishuji-model-loader.elf
DC_MODEL_LOADER_CDI ?= $(DC_BUILD_DIR)/maishuji-model-loader.cdi
DC_HUMAN_MODEL_LOADER_ELF ?= $(DC_BUILD_DIR)/maishuji-human-model-loader.elf
DC_HUMAN_MODEL_LOADER_CDI ?= $(DC_BUILD_DIR)/maishuji-human-model-loader.cdi
DC_MULTI_TEXTURE_ELF ?= $(DC_BUILD_DIR)/maishuji-multi-texture.elf
DC_MULTI_TEXTURE_CDI ?= $(DC_BUILD_DIR)/maishuji-multi-texture.cdi
DC_TILE_WORKLOAD_ELF ?= $(DC_BUILD_DIR)/maishuji-pvr-tile-workload.elf
DC_TILE_WORKLOAD_CDI ?= $(DC_BUILD_DIR)/maishuji-pvr-tile-workload.cdi
DC_FRUSTUM_CULLING_ELF ?= $(DC_BUILD_DIR)/maishuji-frustum-culling.elf
DC_FRUSTUM_CULLING_CDI ?= $(DC_BUILD_DIR)/maishuji-frustum-culling.cdi
KOS_MAKEFILE_BUILD_DIR ?= build-kos-makefile-smoke
MKDCDISC ?= mkdcdisc
# CDI targets own their named output paths, so rebuilding a target replaces
# the previous image by default. Set MKDCDISC_OPTIONS= to restore mkdcdisc's
# refusal behavior when preserving an existing image is intentional.
MKDCDISC_OPTIONS ?= --allow-overwrite
KOS_TOOLCHAIN_FILE ?= /opt/toolchains/dc/kos/utils/cmake/kallistios.toolchain.cmake
CONTAINER_RUNTIME ?= docker

.PHONY: check-toolchain in-container host-configure host-build host-run host-test dreamcast-configure dreamcast-build dreamcast-smoke-cdi dreamcast-lifecycle-cdi dreamcast-textured-cdi dreamcast-pixel-sprites-cdi dreamcast-basic-3d-cdi dreamcast-fogged-3d-cdi dreamcast-lighting-cdi dreamcast-text-cdi dreamcast-particles-cdi dreamcast-budget-cdi dreamcast-advanced-text-cdi dreamcast-pvr-asset-cdi dreamcast-textured-3d-cdi dreamcast-heightmapped-terrain-cdi dreamcast-terrain-walk-cdi dreamcast-mipmap-texture-cdi dreamcast-model-loader-cdi dreamcast-human-model-loader-cdi dreamcast-multi-texture-cdi dreamcast-pvr-tile-workload-cdi dreamcast-frustum-culling-cdi dreamcast-makefile-smoke flycast-smoke flycast-lifecycle flycast-textured-quad flycast-pixel-sprites flycast-basic-3d flycast-fogged-3d flycast-lighting flycast-text flycast-particles flycast-budget flycast-advanced-text flycast-pvr-asset flycast-textured-3d flycast-heightmapped-terrain flycast-terrain-walk flycast-mipmap-texture flycast-model-loader flycast-human-model-loader flycast-multi-texture flycast-pvr-tile-workload flycast-frustum-culling run-dc

# Verify the pinned Dreamcast/KOS toolchain and image versions.
# Use this first when a Dreamcast build fails or after entering the container.
check-toolchain:
	./tools/with-kos.sh ./tools/check-toolchain.sh

# Dispatch a Make target through the pinned KOS container when called from the host.
# Flycast and native host targets stay on the workstation.
in-container:
	CONTAINER_RUNTIME="$(CONTAINER_RUNTIME)" ./tools/in-container.sh "$(TARGET)" $(MAKEOVERRIDES)

# Generate the host CMake build directory in Debug mode.
# Usually invoked automatically by host-build, host-run, and host-test.
host-configure:
	cmake -S . -B $(HOST_BUILD_DIR) -G $(HOST_CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=Debug

# Compile the host library, smoke program, and test executable.
# Use this for fast development checks that do not require Dreamcast headers.
host-build: host-configure
	cmake --build $(HOST_BUILD_DIR) --verbose

# Build and run the small host smoke program.
# Use this to confirm the basic host backend can initialize and shut down.
host-run: host-build
	./$(HOST_BUILD_DIR)/maishuji-host-smoke

# Build and run the host lifecycle and API regression tests.
# Use this after changing framework code or before committing.
host-test: host-build
	./$(HOST_BUILD_DIR)/$(HOST_TEST_EXE)

# Configure the Dreamcast cross-build with the pinned KOS toolchain.
# Use this when changing CMake/toolchain settings; dreamcast-build calls it automatically.
dreamcast-configure: check-toolchain
	./tools/with-kos.sh cmake -S . -B $(DC_BUILD_DIR) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$(DC_BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(KOS_TOOLCHAIN_FILE)

# Compile every Dreamcast ELF target in DC_BUILD_DIR.
# Use DC_BUILD_TYPE=Debug for emulator diagnosis or Release for optimized output;
# CDI, Flycast, and hardware-loader targets depend on this build.
dreamcast-build: dreamcast-configure
	./tools/with-kos.sh cmake --build $(DC_BUILD_DIR) --verbose

# Package the default PVR smoke ELF as a self-booting CDI.
# Override DC_ELF and DC_CDI to package a different program.
dreamcast-smoke-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_ELF)" -o "$(DC_CDI)" -n "maishuji-lab PVR smoke"

dreamcast-lifecycle-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_LIFECYCLE_ELF)" -o "$(DC_LIFECYCLE_CDI)" -n "maishuji-lab hello PVR lifecycle"

dreamcast-textured-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_TEXTURED_ELF)" -o "$(DC_TEXTURED_CDI)" -n "maishuji-lab textured quad"

dreamcast-pixel-sprites-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_PIXEL_SPRITES_ELF)" -o "$(DC_PIXEL_SPRITES_CDI)" -n "maishuji-lab pixel sprites"

dreamcast-basic-3d-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_BASIC_3D_ELF)" -o "$(DC_BASIC_3D_CDI)" -n "maishuji-lab basic 3D"

dreamcast-fogged-3d-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_FOGGED_3D_ELF)" -o "$(DC_FOGGED_3D_CDI)" -n "maishuji-lab fogged 3D"

dreamcast-lighting-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_LIGHTING_ELF)" -o "$(DC_LIGHTING_CDI)" -n "maishuji-lab PVR offset-color lighting"

dreamcast-text-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_TEXT_ELF)" -o "$(DC_TEXT_CDI)" -n "maishuji-lab PVR text"

dreamcast-particles-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_PARTICLES_ELF)" -o "$(DC_PARTICLES_CDI)" -n "maishuji-lab particle batch"

dreamcast-budget-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_BUDGET_ELF)" -o "$(DC_BUDGET_CDI)" -n "maishuji-lab PVR packet budget"

dreamcast-advanced-text-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_ADVANCED_TEXT_ELF)" -o "$(DC_ADVANCED_TEXT_CDI)" -n "maishuji-lab advanced multilingual text"

dreamcast-pvr-asset-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_PVR_ASSET_ELF)" -o "$(DC_PVR_ASSET_CDI)" -n "maishuji-lab PVR asset"

dreamcast-textured-3d-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_TEXTURED_3D_ELF)" -o "$(DC_TEXTURED_3D_CDI)" -n "maishuji-lab textured 3D"

dreamcast-heightmapped-terrain-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_HEIGHTMAPPED_TERRAIN_ELF)" -o "$(DC_HEIGHTMAPPED_TERRAIN_CDI)" -n "maishuji-lab heightmapped terrain"

dreamcast-terrain-walk-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_TERRAIN_WALK_ELF)" -o "$(DC_TERRAIN_WALK_CDI)" -n "maishuji-lab terrain walk"

dreamcast-mipmap-texture-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_MIPMAP_TEXTURE_ELF)" -o "$(DC_MIPMAP_TEXTURE_CDI)" -n "maishuji-lab mipmap texture"

dreamcast-model-loader-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_MODEL_LOADER_ELF)" -o "$(DC_MODEL_LOADER_CDI)" -n "maishuji-lab model loader"

dreamcast-human-model-loader-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_HUMAN_MODEL_LOADER_ELF)" -o "$(DC_HUMAN_MODEL_LOADER_CDI)" -n "maishuji-lab human model loader"

dreamcast-multi-texture-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_MULTI_TEXTURE_ELF)" -o "$(DC_MULTI_TEXTURE_CDI)" -n "maishuji-lab multi-texture"

dreamcast-pvr-tile-workload-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_TILE_WORKLOAD_ELF)" -o "$(DC_TILE_WORKLOAD_CDI)" -n "maishuji-lab PVR tile workload"

dreamcast-frustum-culling-cdi: dreamcast-build
	./tools/with-kos.sh "$(MKDCDISC)" $(MKDCDISC_OPTIONS) -e "$(DC_FRUSTUM_CULLING_ELF)" -o "$(DC_FRUSTUM_CULLING_CDI)" -n "maishuji-lab frustum culling"

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

flycast-textured-3d:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-textured-3d-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_TEXTURED_3D FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: textured 3D passed (PVRT ARGB4444; rotating cube; 12 triangles)" ./tools/test-flycast-render.sh "$(DC_TEXTURED_3D_CDI)"

flycast-heightmapped-terrain:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-heightmapped-terrain-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_HEIGHTMAPPED_TERRAIN FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: heightmapped terrain passed (256x256 ARGB4444; 81 vertices; 128 triangles)" ./tools/test-flycast-render.sh "$(DC_HEIGHTMAPPED_TERRAIN_CDI)"

flycast-terrain-walk:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-terrain-walk-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_TERRAIN_WALK FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: terrain walk passed (controller movement; height sampling; 81 vertices; 128 triangles)" ./tools/test-flycast-render.sh "$(DC_TERRAIN_WALK_CDI)"

flycast-mipmap-texture:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-mipmap-texture-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_MIPMAP_TEXTURE FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: mipmap texture passed (9 levels; 174768 bytes; bilinear filtering)" ./tools/test-flycast-render.sh "$(DC_MIPMAP_TEXTURE_CDI)"

flycast-model-loader:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-model-loader-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_MODEL_LOADER FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: model loader passed (DCM1;" ./tools/test-flycast-render.sh "$(DC_MODEL_LOADER_CDI)"

flycast-human-model-loader:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-model-loader-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_HUMAN_MODEL_LOADER FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: model loader passed (DCM1; 633 vertices; 866 triangles; 64x64 ARGB4444)" ./tools/test-flycast-render.sh "$(DC_HUMAN_MODEL_LOADER_CDI)"

flycast-multi-texture:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-multi-texture-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_MULTI_TEXTURE FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: multi-texture passed (2 textures; satellite body;" ./tools/test-flycast-render.sh "$(DC_MULTI_TEXTURE_CDI)"

flycast-pvr-tile-workload:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-pvr-tile-workload-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_PVR_TILE_WORKLOAD FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: tile workload passed (friendly refs=" ./tools/test-flycast-render.sh "$(DC_TILE_WORKLOAD_CDI)"

flycast-frustum-culling:
	FLYCAST_FRAME_CHECKER=tools/check-flycast-frustum-culling-frame.sh FLYCAST_WINDOW_TITLE=MAISHUJI_FRUSTUM_CULLING FLYCAST_STABLE_SAMPLES=3 FLYCAST_REQUIRED_RUNTIME_MARKER="maishuji: frustum culling passed (mode=on; submitted=2 objects; input=8 triangles" ./tools/test-flycast-render.sh "$(DC_FRUSTUM_CULLING_CDI)"

run-dc: dreamcast-build
	@test -n "$(DC_IP)" || (echo "Set DC_IP to the Dreamcast BBA address, for example: make run-dc DC_IP=YOUR_DREAMCAST_IP" >&2; exit 2)
	./tools/with-kos.sh dc-tool-ip -t "$(DC_IP)" -x "$(DC_ELF)"
