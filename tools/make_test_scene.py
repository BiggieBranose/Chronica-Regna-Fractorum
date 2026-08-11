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


BOX_FACES = [
    [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)],   # +Z
    [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)],  # -Z
    [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)],   # +X
    [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)],  # -X
    [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)],   # +Y
    [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)],  # -Y
]
BOX_NORMALS = [
    (0, 0, 1), (0, 0, -1), (1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0),
]
UVS = [(0, 0), (1, 0), (1, 1), (0, 1)]


def build_box():
    positions = []
    texcoords = []
    normals = []
    indices = []
    for face, normal in zip(BOX_FACES, BOX_NORMALS):
        base = len(positions) // 3
        for p, uv in zip(face, UVS):
            positions.extend(p)
            texcoords.extend(uv)
            normals.extend(normal)
        indices += [base + 0, base + 1, base + 2, base + 0, base + 2, base + 3]
    return positions, texcoords, normals, indices


def build_ground():
    size = 10.0
    positions = [
        -size, 0.0, -size,
        size, 0.0, -size,
        size, 0.0, size,
        -size, 0.0, size,
    ]
    uv_scale = 10.0
    texcoords = [
        0.0, 0.0,
        uv_scale, 0.0,
        uv_scale, uv_scale,
        0.0, uv_scale,
    ]
    normals = [0.0, 1.0, 0.0] * 4
    indices = [0, 2, 1, 0, 3, 2]
    return positions, texcoords, normals, indices


png = make_png()
box_pos, box_uv, box_nrm, box_idx = build_box()
ground_pos, ground_uv, ground_nrm, ground_idx = build_ground()

buffer_data = bytearray()


def add_view(data):
    offset = len(buffer_data)
    buffer_data.extend(data)
    while len(buffer_data) % 4 != 0:
        buffer_data.append(0)
    return offset, len(data)


def pack_floats(values):
    return struct.pack('<%df' % len(values), *values)


def pack_indices(values):
    return struct.pack('<%dH' % len(values), *values)


views = {}

views['ground_pos'] = add_view(pack_floats(ground_pos))
views['ground_uv'] = add_view(pack_floats(ground_uv))
views['ground_nrm'] = add_view(pack_floats(ground_nrm))
views['ground_idx'] = add_view(pack_indices(ground_idx))
views['box_pos'] = add_view(pack_floats(box_pos))
views['box_uv'] = add_view(pack_floats(box_uv))
views['box_nrm'] = add_view(pack_floats(box_nrm))
views['box_idx'] = add_view(pack_indices(box_idx))
views['png'] = add_view(png)

gltf = {
    "asset": {"version": "2.0", "generator": "crf-test-scene"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1, 2, 3, 4, 5, 6, 7]}],
    "nodes": [
        {"name": "Ground", "mesh": 0},
        {"name": "col_wall_north", "mesh": 1, "translation": [0, 1, -9.5], "scale": [19, 2, 1]},
        {"name": "col_wall_south", "mesh": 1, "translation": [0, 1, 9.5], "scale": [19, 2, 1]},
        {"name": "col_wall_east", "mesh": 1, "translation": [9.5, 1, 0], "scale": [1, 2, 19]},
        {"name": "col_wall_west", "mesh": 1, "translation": [-9.5, 1, 0], "scale": [1, 2, 19]},
        {"name": "trg_door", "mesh": 1, "translation": [0, 1, 0], "scale": [3, 2, 3]},
        {"name": "Prop_1", "mesh": 1, "translation": [3, 0.5, 2], "scale": [1, 1, 1]},
        {"name": "Prop_2", "mesh": 1, "translation": [-4, 0.5, -3], "scale": [1.5, 1, 1]},
    ],
    "meshes": [
        {"primitives": [
            {"attributes": {"POSITION": 0, "TEXCOORD_0": 1, "NORMAL": 2}, "indices": 3, "material": 0}
        ]},
        {"primitives": [
            {"attributes": {"POSITION": 4, "TEXCOORD_0": 5, "NORMAL": 6}, "indices": 7, "material": 0}
        ]},
    ],
    "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"source": 0, "sampler": 0}],
    "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 10497, "wrapT": 10497}],
    "images": [{"bufferView": 8, "mimeType": "image/png"}],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
         "min": [-10, 0, -10], "max": [10, 0, 10]},
        {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC2"},
        {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC3"},
        {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
        {"bufferView": 4, "componentType": 5126, "count": 24, "type": "VEC3",
         "min": [-1, -1, -1], "max": [1, 1, 1]},
        {"bufferView": 5, "componentType": 5126, "count": 24, "type": "VEC2"},
        {"bufferView": 6, "componentType": 5126, "count": 24, "type": "VEC3"},
        {"bufferView": 7, "componentType": 5123, "count": 36, "type": "SCALAR"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": views['ground_pos'][0], "byteLength": views['ground_pos'][1]},
        {"buffer": 0, "byteOffset": views['ground_uv'][0], "byteLength": views['ground_uv'][1]},
        {"buffer": 0, "byteOffset": views['ground_nrm'][0], "byteLength": views['ground_nrm'][1]},
        {"buffer": 0, "byteOffset": views['ground_idx'][0], "byteLength": views['ground_idx'][1]},
        {"buffer": 0, "byteOffset": views['box_pos'][0], "byteLength": views['box_pos'][1]},
        {"buffer": 0, "byteOffset": views['box_uv'][0], "byteLength": views['box_uv'][1]},
        {"buffer": 0, "byteOffset": views['box_nrm'][0], "byteLength": views['box_nrm'][1]},
        {"buffer": 0, "byteOffset": views['box_idx'][0], "byteLength": views['box_idx'][1]},
        {"buffer": 0, "byteOffset": views['png'][0], "byteLength": views['png'][1]},
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

out = r"C:\Projects\Chronica-Regna-Fractorum\assets\models\test_scene.glb"
with open(out, 'wb') as f:
    f.write(glb)

print("Wrote", out)
print(len(box_pos) // 3, "box verts,", len(ground_pos) // 3, "ground verts,", len(glb), "total bytes")
