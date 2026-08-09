import json
import struct
import zlib

W = H = 16
CHECKER_A = (255, 255, 255)
CHECKER_B = (40, 40, 40)


def make_png():
    sig = b'\x89PNG\r\n\x1a\n'

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    ihdr = struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)
    rows = bytearray()
    for y in range(H):
        rows.append(0)
        for x in range(W):
            c = CHECKER_A if (x + y) % 2 == 0 else CHECKER_B
            rows += bytes(c)
    idat = zlib.compress(bytes(rows), 9)
    return sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', idat) + chunk(b'IEND', b'')


FACES = [
    [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)],
    [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)],
    [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)],
    [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)],
    [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)],
    [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)],
]
UVS = [(0, 0), (1, 0), (1, 1), (0, 1)]

positions = []
texcoords = []
indices = []
for f in FACES:
    base = len(positions) // 3
    for p, uv in zip(f, UVS):
        positions.extend(p)
        texcoords.extend(uv)
    indices += [base + 0, base + 1, base + 2, base + 0, base + 2, base + 3]

png = make_png()

buffer_data = bytearray()


def add_view(data):
    offset = len(buffer_data)
    buffer_data.extend(data)
    while len(buffer_data) % 4 != 0:
        buffer_data.append(0)
    return offset, len(data)


pos_offset, pos_len = add_view(struct.pack('<%df' % len(positions), *positions))
uv_offset, uv_len = add_view(struct.pack('<%df' % len(texcoords), *texcoords))
idx_offset, idx_len = add_view(struct.pack('<%dH' % len(indices), *indices))
png_offset, png_len = add_view(png)

gltf = {
    "asset": {"version": "2.0", "generator": "crf-test-cube"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "TestCube", "mesh": 0}],
    "meshes": [{"primitives": [
        {"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "material": 0}
    ]}],
    "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"source": 0, "sampler": 0}],
    "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
    "images": [{"bufferView": 3, "mimeType": "image/png"}],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 24, "type": "VEC3",
         "min": [-1, -1, -1], "max": [1, 1, 1]},
        {"bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC2"},
        {"bufferView": 2, "componentType": 5123, "count": 36, "type": "SCALAR"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": pos_offset, "byteLength": pos_len},
        {"buffer": 0, "byteOffset": uv_offset, "byteLength": uv_len},
        {"buffer": 0, "byteOffset": idx_offset, "byteLength": idx_len},
        {"buffer": 0, "byteOffset": png_offset, "byteLength": png_len},
    ],
    "buffers": [{"byteLength": len(buffer_data)}],
}

json_bytes = json.dumps(gltf, separators=(',', ':')).encode('utf-8')
json_bytes += b' ' * ((4 - len(json_bytes) % 4) % 4)
bin_bytes = bytes(buffer_data)
bin_bytes += b'\x00' * ((4 - len(bin_bytes) % 4) % 4)

total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
glb = struct.pack('<III', 0x46546C67, 2, total)
glb += struct.pack('<I', len(json_bytes)) + b'JSON' + json_bytes
glb += struct.pack('<I', len(bin_bytes)) + b'BIN\x00' + bin_bytes

out = r"C:\Projects\Chronica-Regna-Fractorum\assets\models\test_cube.glb"
with open(out, 'wb') as f:
    f.write(glb)

print("Wrote", out)
print(len(positions) // 3, "vertices,", len(indices), "indices,", png_len, "bytes of embedded PNG,", len(glb), "total bytes")
