# Dreamcast glossary

DMA(pvr):
	Represents direct memory access used to move data without a CPU copy for every word.

PVR(rendering):
	Represents the Dreamcast's PowerVR2 hardware and its tile-based polygon pipeline.

KOS(platform):
	Provides the public operating-system APIs, startup code, and device drivers used by the project.

TA(pvr):
	Receives submitted polygon data and builds the tile lists consumed by the PVR renderer.

VRAM(pvr):
	Stores texture data and PVR-managed rendering buffers in the Dreamcast's video memory.

OPB(pvr):
	Stores per-tile references to opaque, punch-through, and translucent polygon data.

G2(bus):
	Connects the SH-4 system bus to Dreamcast peripherals such as the PVR and AICA.

SH-4(cpu):
	Executes the game and rendering code on the Dreamcast's 32-bit RISC processor.

RenderList(api):
	Represents one active PVR polygon list within a frame and its submission lifetime.

Frame(api):
	Owns one scene submission interval and coordinates its render-list lifetimes.

ROMDISK(asset):
	Embeds application data into the executable image for predictable Dreamcast access.

Flycast(emulator):
	Emulates Dreamcast hardware for development checks when a physical console is unavailable.

CDI(disc-image):
	Packages a bootable Dreamcast program and its assets into an image accepted by emulators or optical-disc tools.
