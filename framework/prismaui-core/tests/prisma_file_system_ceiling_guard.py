from pathlib import Path


source = Path("src/PrismaUI/PrismaFileSystem.cpp").read_text(encoding="utf-8")

assert source.count("constexpr std::size_t kMaximumFileBytes = 64u * 1024u * 1024u;") == 1, (
    "the 64 MiB loader ceiling must be declared exactly once"
)
assert source.count("size > kMaximumFileBytes") == 1, "the ceiling must reject only sizes strictly above 64 MiB"

open_file = source[source.index("PrismaFileSystem::OpenFile("):source.index("std::vector<char> bytes(")]
assert "size > kMaximumFileBytes" in open_file, "OpenFile must reject oversize files before reading them"
oversize_branch = open_file[open_file.index("if (size > kMaximumFileBytes)"):]
assert "logger::warn(" in oversize_branch[:oversize_branch.index("return {};")], (
    "oversize rejections must be logged inside the ceiling branch"
)

print("Prisma file-system ceiling guard passed")
