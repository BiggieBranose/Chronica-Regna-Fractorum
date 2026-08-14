import json
import struct
import zlib

W, H = 32, 64
BG = (32, 36, 48)
SKIN = (240, 200, 160)
JACKET = (90, 130, 200)
PANTS = (70, 70, 90)
BOOTS = (45, 45, 60)


def make_png():
    px = [[BG] * W for _ in range(H)]

    def circle(cx, cy, r, color):
        for y in range(H):
            for x in range(W):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    px[y][x] = color

    def rect(x0, x1, y0, y1, color):
        for y in range(max(0, y0), min(H, y1)):
            for x in range(max(0, x0), min(W, x1)):
                px[y][x] = color

    circle(16, 13, 6, SKIN)          # head
    rect(11, 21, 22, 41, JACKET)     # torso
    rect(7, 11, 23, 40, JACKET)      # left arm
    rect(21, 25, 23, 40, JACKET)     # right arm
    rect(13, 16, 41, 58, PANTS)      # left leg
    rect(16, 19, 41, 58, PANTS)      # right leg
    rect(13, 16, 58, 63, BOOTS)      # left boot
    rect(16, 19, 58, 63, BOOTS)      # right boot

    rows = bytearray()
    for y in range(H):
        rows.append(0)
        for x in range(W):
            r, g, b = px[y][x]
            rows += bytes((r, g, b, 255))

    sig = b"\x89PNG\r\n\x1a\n"

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(rows), 9)
    return sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")


positions = [0, 0, -0.5, 0, 0, 0.5, 0, 2, 0.5, 0, 2, -0.5]
texcoords = [1, 1, 0, 1, 0, 0, 1, 0]
normals = [0, 1, 0] * 4
indices = [0, 2, 1, 0, 3, 2]

png = make_png()

buffer_data = bytearray()


def add_view(data):
    offset = len(buffer_data)
    buffer_data.extend(data)
    while len(buffer_data) % 4 != 0:
        buffer_data.append(0)
    return offset, len(data)


pos_offset, pos_len = add_view(struct.pack("<%df" % len(positions), *positions))
uv_offset, uv_len = add_view(struct.pack("<%df" % len(texcoords), *texcoords))
nrm_offset, nrm_len = add_view(struct.pack("<%df" % len(normals), *normals))
idx_offset, idx_len = add_view(struct.pack("<%dH" % len(indices), *indices))
png_offset, png_len = add_view(png)

gltf = {
    "asset": {"version": "2.0", "generator": "crf-player-sprite"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "PlayerSprite", "mesh": 0}],
    "meshes": [{"primitives": [
        {"attributes": {"POSITION": 0, "TEXCOORD_0": 1, "NORMAL": 2}, "indices": 3, "material": 0}
    ]}],
    "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"source": 0, "sampler": 0}],
    "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
    "images": [{"bufferView": 4, "mimeType": "image/png"}],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
         "min": [0, 0, -0.5], "max": [0, 2, 0.5]},
        {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC2"},
        {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC3"},
        {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": pos_offset, "byteLength": pos_len},
        {"buffer": 0, "byteOffset": uv_offset, "byteLength": uv_len},
        {"buffer": 0, "byteOffset": nrm_offset, "byteLength": nrm_len},
        {"buffer": 0, "byteOffset": idx_offset, "byteLength": idx_len},
        {"buffer": 0, "byteOffset": png_offset, "byteLength": png_len},
    ],
    "buffers": [{"byteLength": len(buffer_data)}],
}

json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
bin_bytes = bytes(buffer_data)
bin_bytes += b"\x00" * ((4 - len(bin_bytes) % 4) % 4)

total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
glb = struct.pack("<III", 0x46546C67, 2, total)
glb += struct.pack("<I", len(json_bytes)) + b"JSON" + json_bytes
glb += struct.pack("<I", len(bin_bytes)) + b"BIN\x00" + bin_bytes

out = r"C:\Projects\Chronica-Regna-Fractorum\assets\models\player_sprite.glb"
with open(out, "wb") as f:
    f.write(glb)

print("Wrote", out)
print("quad verts:", len(positions) // 3, "indices:", len(indices), "sprite:", W, "x", H, "glb:", len(glb), "bytes")
