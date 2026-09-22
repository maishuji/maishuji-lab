# Minimal native KOS Makefile validation for the raw PVR reference.
# CMake remains the project build; this target checks the supplied KOS rules
# and compiler/linker wrappers against the same source and flags.

BUILD_DIR ?= build-kos-makefile-smoke
TARGET := $(BUILD_DIR)/maishuji-pvr-smoke.elf
OBJECT := $(BUILD_DIR)/pvr_smoke.o

CXXFLAGS += -std=c++20 -fno-exceptions -fno-rtti

.PHONY: all clean

all: $(TARGET)

include $(KOS_BASE)/Makefile.rules

$(TARGET): $(OBJECT)
	@mkdir -p "$(BUILD_DIR)"
	kos-c++ -fno-lto $< -o $@

$(OBJECT): examples/pvr_smoke.cpp
	@mkdir -p "$(BUILD_DIR)"
	kos-c++ $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf "$(BUILD_DIR)"
