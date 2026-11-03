#pragma once

#if defined(PRISMA_DIAGNOSTICS)

#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace ControllerProbeDiagnostics {

inline bool Sha256Hex(const std::uint8_t* data, std::size_t size, std::string& outHex) noexcept {
    outHex.clear();
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    DWORD hashObjLen = 0, hashLen = 0, cbData = 0;
    bool ok = false;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjLen), sizeof(hashObjLen), &cbData, 0) == 0 &&
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cbData, 0) == 0) {
        std::vector<std::uint8_t> hashObj(hashObjLen);
        std::vector<std::uint8_t> hash(hashLen);
        BCRYPT_HASH_HANDLE hashHandle = nullptr;
        if (BCryptCreateHash(alg, &hashHandle, hashObj.data(), hashObjLen, nullptr, 0, 0) == 0) {
            if (BCryptHashData(hashHandle, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0) == 0 &&
                BCryptFinishHash(hashHandle, hash.data(), hashLen, 0) == 0) {
                static const char* hexDigits = "0123456789abcdef";
                outHex.resize(static_cast<std::size_t>(hashLen) * 2);
                for (DWORD i = 0; i < hashLen; ++i) {
                    outHex[i * 2] = hexDigits[hash[i] >> 4];
                    outHex[i * 2 + 1] = hexDigits[hash[i] & 0xF];
                }
                ok = true;
            }
            BCryptDestroyHash(hashHandle);
        }
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

inline bool ContainsBytes(const std::uint8_t* haystack, std::size_t haystackLen, const char* needle) noexcept {
    const std::size_t needleLen = std::strlen(needle);
    if (needleLen == 0 || needleLen > haystackLen) return false;
    for (std::size_t i = 0; i + needleLen <= haystackLen; ++i)
        if (std::memcmp(haystack + i, needle, needleLen) == 0) return true;
    return false;
}

struct ExportTrace {
    bool directoryValid = false;
    std::uint32_t base = 0;
    std::uint32_t numberOfNames = 0;
    bool namesSorted = false;
    bool found = false;
    std::int32_t nameTableIndex = -1;
    std::uint16_t unbiasedIndex = 0;
    std::uint32_t publicOrdinal = 0;
    std::uint32_t functionRVA = 0;
    char section[9]{};
};

inline const char* SectionAt(const IMAGE_NT_HEADERS64* nt, std::uint32_t rva) noexcept {
    static char name[9];
    std::memset(name, 0, sizeof(name));
    const auto* sectionHeaders = IMAGE_FIRST_SECTION(nt);
    for (std::uint16_t s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        const auto& section = sectionHeaders[s];
        const auto sectionSize = section.Misc.VirtualSize > section.SizeOfRawData ? section.Misc.VirtualSize : section.SizeOfRawData;
        if (rva >= section.VirtualAddress && rva < section.VirtualAddress + sectionSize) {
            std::memcpy(name, section.Name, 8);
            break;
        }
    }
    return name;
}

inline ExportTrace TraceExport(HMODULE module, const char* targetName) noexcept {
    ExportTrace trace{};
    logger::info("[ControllerProbe] TraceExport target={} unverified=true", targetName);
    if (!module) return trace;
    const auto* imageBase = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return trace;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return trace;
    const auto& exportDataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDataDir.VirtualAddress == 0) return trace;
    const auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(imageBase + exportDataDir.VirtualAddress);
    const auto* names = reinterpret_cast<const std::uint32_t*>(imageBase + exportDir->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const std::uint16_t*>(imageBase + exportDir->AddressOfNameOrdinals);
    const auto* functions = reinterpret_cast<const std::uint32_t*>(imageBase + exportDir->AddressOfFunctions);
    trace.directoryValid = true;
    trace.base = exportDir->Base;
    trace.numberOfNames = exportDir->NumberOfNames;
    trace.namesSorted = true;
    const char* previousName = nullptr;
    for (std::uint32_t i = 0; i < exportDir->NumberOfNames; ++i) {
        const char* currentName = reinterpret_cast<const char*>(imageBase + names[i]);
        if (previousName && std::strcmp(previousName, currentName) > 0) trace.namesSorted = false;
        previousName = currentName;
        if (!trace.found && std::strcmp(currentName, targetName) == 0) {
            trace.found = true;
            trace.nameTableIndex = static_cast<std::int32_t>(i);
            trace.unbiasedIndex = ordinals[i];
            trace.publicOrdinal = static_cast<std::uint32_t>(trace.unbiasedIndex) + trace.base;
            trace.functionRVA = functions[trace.unbiasedIndex];
        }
    }
    if (trace.found) std::memcpy(trace.section, SectionAt(nt, trace.functionRVA), 8);
    return trace;
}

inline void LogAllExports(HMODULE module) noexcept {
    if (!module) return;
    const auto* imageBase = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    const auto& exportDataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDataDir.VirtualAddress == 0) return;
    const auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(imageBase + exportDataDir.VirtualAddress);
    const auto* names = reinterpret_cast<const std::uint32_t*>(imageBase + exportDir->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const std::uint16_t*>(imageBase + exportDir->AddressOfNameOrdinals);
    const auto* functions = reinterpret_cast<const std::uint32_t*>(imageBase + exportDir->AddressOfFunctions);
    logger::info("[ControllerProbe] in-memory export dump (unverified vs disk) base={} numberOfNames={}", exportDir->Base, exportDir->NumberOfNames);
    for (std::uint32_t i = 0; i < exportDir->NumberOfNames; ++i) {
        const char* currentName = reinterpret_cast<const char*>(imageBase + names[i]);
        const std::uint16_t unbiasedIndex = ordinals[i];
        const std::uint32_t publicOrdinal = static_cast<std::uint32_t>(unbiasedIndex) + exportDir->Base;
        const std::uint32_t rva = functions[unbiasedIndex];
        logger::info("[ControllerProbe] in-memory export {} name={} unbiasedIndex={} publicOrdinal={} rva={:#x} section={} unverified={}",
                     i, currentName, unbiasedIndex, publicOrdinal, rva, SectionAt(nt, rva), true);
    }
}

inline void LogMemoryProtection(HMODULE module) noexcept {
    if (!module) return;
    const auto* imageBase = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    const auto& exportDataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDataDir.VirtualAddress == 0) return;
    const auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(imageBase + exportDataDir.VirtualAddress);
    const auto* namesArray = imageBase + exportDir->AddressOfNames;
    MEMORY_BASIC_INFORMATION dirInfo{};
    MEMORY_BASIC_INFORMATION namesInfo{};
    VirtualQuery(exportDir, &dirInfo, sizeof(dirInfo));
    VirtualQuery(namesArray, &namesInfo, sizeof(namesInfo));
    logger::info("[ControllerProbe] memory protection unverified=true exportDirProtect={:#x} exportDirState={:#x} exportDirBase={} namesArrayProtect={:#x} namesArrayState={:#x} namesArrayBase={}",
                 dirInfo.Protect, dirInfo.State, reinterpret_cast<std::uintptr_t>(dirInfo.AllocationBase),
                 namesInfo.Protect, namesInfo.State, reinterpret_cast<std::uintptr_t>(namesInfo.AllocationBase));
}

struct DiskExportSummary {
    bool opened = false;
    bool parsed = false;
    std::uint64_t fileSize = 0;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint32_t exportRVA = 0;
    std::uint32_t exportSize = 0;
    std::uint32_t numberOfNames = 0;
    bool found = false;
    bool containsGetApiString = false;
    bool containsRetiredString = false;
    std::string sha256;
    std::string textSectionSha256;
};

inline std::size_t RvaToFileOffsetBounded(const IMAGE_NT_HEADERS64* nt, std::uint32_t rva, std::size_t bufferSize) noexcept {
    const auto* sectionHeaders = IMAGE_FIRST_SECTION(nt);
    for (std::uint16_t s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        const auto& section = sectionHeaders[s];
        const std::uint32_t virtualExtent = section.Misc.VirtualSize > section.SizeOfRawData ? section.Misc.VirtualSize : section.SizeOfRawData;
        if (rva < section.VirtualAddress || rva >= section.VirtualAddress + virtualExtent) continue;
        const std::uint32_t withinSection = rva - section.VirtualAddress;
        if (withinSection >= section.SizeOfRawData) return 0;
        if (static_cast<std::uint64_t>(section.PointerToRawData) + section.SizeOfRawData > bufferSize) return 0;
        const std::size_t offset = static_cast<std::size_t>(section.PointerToRawData) + withinSection;
        return offset < bufferSize ? offset : 0;
    }
    return 0;
}

inline bool BoundedStringEquals(const std::uint8_t* base, std::size_t bufferSize, std::size_t offset,
                                 const char* target, std::size_t maxLen = 512) noexcept {
    if (offset >= bufferSize) return false;
    const std::size_t limit = std::min(bufferSize - offset, maxLen);
    const char* s = reinterpret_cast<const char*>(base + offset);
    std::size_t len = 0;
    while (len < limit && s[len] != '\0') ++len;
    if (len >= limit) return false;
    return std::strcmp(s, target) == 0;
}

inline DiskExportSummary TraceExportFromDisk(const wchar_t* path, const char* targetName) noexcept {
    DiskExportSummary summary{};
    if (!path || !path[0]) return summary;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return summary;
    summary.opened = true;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > (64ll * 1024 * 1024)) {
        CloseHandle(file);
        return summary;
    }
    summary.fileSize = static_cast<std::uint64_t>(size.QuadPart);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size.QuadPart));
    DWORD totalRead = 0;
    while (totalRead < buffer.size()) {
        DWORD chunk = 0;
        if (!ReadFile(file, buffer.data() + totalRead, static_cast<DWORD>(buffer.size() - totalRead), &chunk, nullptr) || chunk == 0) break;
        totalRead += chunk;
    }
    CloseHandle(file);
    if (totalRead != buffer.size()) return summary;

    const auto* fileBase = buffer.data();
    Sha256Hex(fileBase, buffer.size(), summary.sha256);
    summary.containsGetApiString = ContainsBytes(fileBase, buffer.size(), "PrismaUI_F4_GetAPI");
    summary.containsRetiredString = ContainsBytes(fileBase, buffer.size(),
        "V12 controller API retired; use IVPrismaUI9 or IVPrismaUI11");

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(fileBase);
    if (buffer.size() < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE) return summary;
    if (static_cast<std::uint64_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > buffer.size()) return summary;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(fileBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return summary;
    summary.timeDateStamp = nt->FileHeader.TimeDateStamp;
    summary.sizeOfImage = nt->OptionalHeader.SizeOfImage;

    const auto* sectionHeaders = IMAGE_FIRST_SECTION(nt);
    if (reinterpret_cast<const std::uint8_t*>(sectionHeaders + nt->FileHeader.NumberOfSections) >
        fileBase + buffer.size()) return summary;
    for (std::uint16_t s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        const auto& section = sectionHeaders[s];
        if (std::memcmp(section.Name, ".text", 5) != 0) continue;
        const std::uint64_t end = static_cast<std::uint64_t>(section.PointerToRawData) + section.SizeOfRawData;
        if (section.SizeOfRawData == 0 || end > buffer.size()) break;
        Sha256Hex(fileBase + section.PointerToRawData, section.SizeOfRawData, summary.textSectionSha256);
        break;
    }

    const auto& exportDataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    summary.exportRVA = exportDataDir.VirtualAddress;
    summary.exportSize = exportDataDir.Size;
    if (exportDataDir.VirtualAddress == 0) return summary;

    const std::size_t exportDirOffset = RvaToFileOffsetBounded(nt, exportDataDir.VirtualAddress, buffer.size());
    if (exportDirOffset == 0 || exportDirOffset + sizeof(IMAGE_EXPORT_DIRECTORY) > buffer.size()) return summary;
    const auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(fileBase + exportDirOffset);
    summary.parsed = true;
    summary.numberOfNames = exportDir->NumberOfNames;

    const std::size_t namesOffset = RvaToFileOffsetBounded(nt, exportDir->AddressOfNames, buffer.size());
    if (namesOffset == 0) return summary;
    const std::uint64_t namesTableBytes = static_cast<std::uint64_t>(exportDir->NumberOfNames) * sizeof(std::uint32_t);
    if (namesOffset + namesTableBytes > buffer.size()) return summary;
    const auto* names = reinterpret_cast<const std::uint32_t*>(fileBase + namesOffset);
    for (std::uint32_t i = 0; i < exportDir->NumberOfNames; ++i) {
        const std::size_t nameOffset = RvaToFileOffsetBounded(nt, names[i], buffer.size());
        if (nameOffset == 0) continue;
        if (BoundedStringEquals(fileBase, buffer.size(), nameOffset, targetName)) {
            summary.found = true;
            break;
        }
    }
    return summary;
}

struct MemoryImageIdentity {
    bool valid = false;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint32_t exportRVA = 0;
    std::uint32_t exportSize = 0;
    std::string textSectionSha256;
};

inline MemoryImageIdentity LogImageIdentity(HMODULE module) noexcept {
    MemoryImageIdentity identity{};
    if (!module) return identity;
    const auto* imageBase = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return identity;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return identity;
    identity.valid = true;
    identity.timeDateStamp = nt->FileHeader.TimeDateStamp;
    identity.sizeOfImage = nt->OptionalHeader.SizeOfImage;
    const auto& exportDataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    identity.exportRVA = exportDataDir.VirtualAddress;
    identity.exportSize = exportDataDir.Size;
    const auto* sectionHeaders = IMAGE_FIRST_SECTION(nt);
    for (std::uint16_t s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        const auto& section = sectionHeaders[s];
        if (std::memcmp(section.Name, ".text", 5) != 0) continue;
        if (section.Misc.VirtualSize == 0) break;
        Sha256Hex(imageBase + section.VirtualAddress, section.Misc.VirtualSize, identity.textSectionSha256);
        break;
    }
    logger::info("[ControllerProbe] memory image identity unverified=true timeDateStamp={:#x} sizeOfImage={:#x} exportRVA={:#x} exportSize={:#x} textSectionSha256={}",
                 identity.timeDateStamp, identity.sizeOfImage, identity.exportRVA, identity.exportSize,
                 identity.textSectionSha256.empty() ? "n/a" : identity.textSectionSha256);
    return identity;
}

}

#endif
