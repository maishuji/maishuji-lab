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

Gouraud(pvr):
	Interpolates vertex colors across a polygon so the PVR shades each pixel from the submitted vertices.

Culling(pvr):
	Rejects polygons based on their winding before the PVR rasterizes them.

Triangle-strip(pvr):
	Reuses each pair of recent vertices so a sequence of vertices describes connected triangles.

Polygon-header(pvr):
	Describes the PVR list and rendering state that precede a primitive's vertex packets.


ARGB4444(texture):
	Stores four-bit alpha, red, green, and blue channels in one 16-bit PVR texel.

Twiddling(pvr):
	Reorders texture addresses for PVR locality; the supported texture path uses non-twiddled row-major data.

Pixel-snapping(api):
	Rounds a coordinate to the nearest logical pixel before pixel-art scaling or submission.

Logical-grid(api):
	Maps a smaller design resolution to a fixed output resolution while preserving integer pixel blocks.

UV-coordinate(texture):
	Selects a normalized position inside a texture for a submitted vertex.

Sprite-region(api):
	Defines the normalized UV rectangle and logical bounds used to build a textured quad.
