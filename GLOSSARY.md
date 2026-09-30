# Dreamcast glossary

IgnoreTexA(pvr):
	Forces sampled texture alpha to opaque when set; KOS exposes this inverted flag as txr.alpha.

PT_ALPHA_REF(pvr):
	Defines the global alpha threshold below which punch-through texels are discarded.

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

Depth-compare(pvr):
	Selects the depth comparison that decides whether a polygon passes.

Depth-write(pvr):
	Controls whether a passing polygon updates the PVR depth buffer.

Fog(api):
	Blends mesh vertex colors toward a configured color by camera-space depth.

Camera-space-depth(math):
	Measures positive distance along the camera view direction after the view transform.

Triangle-strip(pvr):
	Reuses each pair of recent vertices so a sequence of vertices describes connected triangles.

Polygon-header(pvr):
	Describes the PVR list and rendering state that precede a primitive's vertex packets.


ARGB4444(texture):
	Stores four-bit alpha, red, green, and blue channels in one 16-bit PVR texel.

Twiddling(pvr):
	Reorders texture addresses for PVR locality; the supported texture path uses non-twiddled row-major data.

PVRT(pvr):
	Stores a PowerVR texture container header and its encoded texel payload.

Morton-order(pvr):
	Maps two-dimensional texture coordinates into the interleaved address order used by a twiddled texture.

Pixel-snapping(api):
	Rounds a coordinate to the nearest logical pixel before pixel-art scaling or submission.

Logical-grid(api):
	Maps a smaller design resolution to a fixed output resolution while preserving integer pixel blocks.

UV-coordinate(texture):
	Selects a normalized position inside a texture for a submitted vertex.

Sprite-region(api):
	Defines the texture-space rectangle used to derive normalized UVs for a textured quad.

Texture-atlas(texture):
	Packs multiple sprite images into one texture allocation for shared sampling state.

Subpixel-motion(api):
	Preserves fractional output coordinates instead of snapping to the logical pixel grid.

NDC(math):
	Represents normalized device coordinates after perspective division and before viewport mapping.

View-matrix(math):
	Converts world-space positions into coordinates relative to a camera.

Projection-matrix(math):
	Maps camera-space positions into clip space for perspective projection.

Clip-space(math):
	Represents homogeneous coordinates before perspective division and viewport mapping.

Viewport(api):
	Defines the output rectangle used to map normalized device coordinates to screen positions.

Mesh(api):
	Describes span-backed indexed vertices that become submitted PVR triangles.

Textured-mesh(api):
	Represents an indexed mesh whose vertices carry UV coordinates for textured PVR triangles.

Heightmap(api):
	Stores scalar elevation samples that become terrain vertex heights.

UV-tiling(texture):
	Repeats normalized texture coordinates across a mesh surface.

OARGB(pvr):
	Stores the packed per-vertex offset color used by the PVR polygon packet.

Offset-color(pvr):
	Represents an additive per-vertex color used by PVR polygon shading.

Specular-lighting(pvr):
	Enables PVR offset-color processing without defining a general light model.

BFont(kos):
	Provides the Dreamcast BIOS bitmap-font rasterization functions used to build text pixels.

Text-texture(pvr):
	Stores rasterized glyph pixels in PVR texture memory so text can be submitted as a textured quad.

Particle-batch(api):
	Groups CPU-defined particle quads into one render-list submission interval while keeping each PVR primitive explicit.

Packet-budget(pvr):
	Counts polygon headers, vertex packets, and primitive calls in a source-derived submission workload.

Vertex-buffer(pvr):
	Stores submitted PVR headers and vertices before the renderer consumes them; its high-water use is not inferred from packet counts alone.

Glyph-atlas(texture):
	Packs pre-rasterized character images into one texture so text can reuse one PVR allocation.

UTF-8(api):
	Encodes Unicode code points as variable-length bytes that a text path must decode before glyph lookup.
