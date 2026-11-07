import argparse
import struct
from pathlib import Path


def export_names(data: bytes, pe_offset: int, optional_size: int) -> set[str]:
    optional = pe_offset + 24
    export_rva = struct.unpack_from("<I", data, optional + 112)[0]
    if export_rva == 0:
        return set()

    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    sections = []
    for index in range(section_count):
        offset = optional + optional_size + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, offset + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer))

    def rva_to_offset(rva: int) -> int:
        for virtual_address, size, raw_pointer in sections:
            if virtual_address <= rva < virtual_address + size:
                return raw_pointer + rva - virtual_address
        raise ValueError(f"RVA 0x{rva:X} is outside PE sections")

    directory = rva_to_offset(export_rva)
    name_count = struct.unpack_from("<I", data, directory + 24)[0]
    names_rva = struct.unpack_from("<I", data, directory + 32)[0]
    names_offset = rva_to_offset(names_rva)
    names = set()
    for index in range(name_count):
        name_rva = struct.unpack_from("<I", data, names_offset + index * 4)[0]
        start = rva_to_offset(name_rva)
        end = data.index(b"\0", start)
        names.add(data[start:end].decode("ascii"))
    return names


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--require-export", action="append", default=[])
    parser.add_argument("--forbid-export", action="append", default=[])
    args = parser.parse_args()

    data = args.executable.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise SystemExit(f"{args.executable}: not an MZ executable")
    pe_offset = struct.unpack_from("<I", data, 60)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise SystemExit(f"{args.executable}: not a PE executable")

    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
    if machine != 0x8664 or optional_magic != 0x20B:
        raise SystemExit(f"{args.executable}: not a PE32+ x86-64 executable")

    try:
        exports = export_names(data, pe_offset, optional_size)
    except (IndexError, struct.error, UnicodeDecodeError, ValueError) as error:
        raise SystemExit(f"{args.executable}: invalid export table: {error}") from error

    missing = sorted(set(args.require_export) - exports)
    forbidden = sorted(set(args.forbid_export) & exports)
    if missing:
        raise SystemExit(f"{args.executable}: missing exports: {', '.join(missing)}")
    if forbidden:
        raise SystemExit(f"{args.executable}: forbidden exports: {', '.join(forbidden)}")

    print(f"{args.executable}: PE32+ x86-64, {len(exports)} named exports")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
