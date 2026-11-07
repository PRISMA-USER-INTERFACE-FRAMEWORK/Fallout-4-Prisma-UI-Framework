#include <cstdint>
#include <cstdio>
#include <vector>
#include "PrismaUI/PartialUpload.h"

using PrismaUI::PartialUpload::Box;
using PrismaUI::PartialUpload::Clamp;
using PrismaUI::PartialUpload::CopyBox;
using PrismaUI::PartialUpload::Full;
using PrismaUI::PartialUpload::Merge;
using PrismaUI::PartialUpload::Offset;
using PrismaUI::PartialUpload::Union;
using PrismaUI::PartialUpload::UploadBox;

static int failures = 0;

static void Expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

int main() {
    Expect(Clamp(-5, -5, 50, 40, 32, 16) == Full(32, 16), "dirty bounds past every edge clamp to the whole view");
    Expect(Clamp(4, 2, 10, 6, 32, 16) == (Box{4, 2, 10, 6}), "dirty bounds inside the view stay as they are");
    Expect(Clamp(40, 2, 50, 6, 32, 16).Empty(), "dirty bounds entirely outside the view are empty");
    Expect(Clamp(8, 8, 8, 12, 32, 16).Empty(), "a zero-width rect is empty");

    const Box a{2, 3, 6, 5};
    const Box b{10, 1, 12, 4};
    Expect(Union(Box{}, a) == a && Union(a, Box{}) == a, "the empty box is the identity of Union");
    Expect(Union(a, b) == (Box{2, 1, 12, 5}), "Union covers both boxes");

    const std::uint32_t width = 8, height = 6, rowBytes = width * 4 + 16;
    std::vector<std::uint8_t> src(static_cast<std::size_t>(rowBytes) * height);
    for (std::size_t i = 0; i < src.size(); ++i) src[i] = static_cast<std::uint8_t>(i * 7 + 1);
    std::vector<std::uint8_t> dst(src.size(), 0);
    const Box box{2, 1, 5, 4};
    CopyBox(dst.data(), src.data(), rowBytes, box);
    bool insideCopied = true, outsideUntouched = true;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < rowBytes; ++x) {
            const std::size_t at = static_cast<std::size_t>(y) * rowBytes + x;
            const bool inside = y >= box.top && y < box.bottom && x >= box.left * 4 && x < box.right * 4;
            if (inside && dst[at] != src[at]) insideCopied = false;
            if (!inside && dst[at] != 0) outsideUntouched = false;
        }
    }
    Expect(insideCopied, "CopyBox copies every byte inside the box");
    Expect(outsideUntouched, "CopyBox leaves every byte outside the box alone");
    Expect(Offset(box, rowBytes) == static_cast<std::size_t>(1) * rowBytes + 2 * 4,
           "Offset points at the box's first pixel");

    std::vector<std::uint8_t> unchanged(src.size(), 9);
    CopyBox(unchanged.data(), src.data(), rowBytes, Box{});
    Expect(unchanged == std::vector<std::uint8_t>(src.size(), 9), "an empty box copies nothing");

    // Two publishes before one upload: both changed areas must reach the published copy and the upload box.
    std::vector<std::uint8_t> published(src.size(), 0);
    std::vector<std::uint8_t> shadow = src;
    Box pending{};
    const Box first{0, 0, 2, 2};
    const Box second{6, 4, 8, 6};
    Expect(Merge(published, pending, shadow, rowBytes, height, first), "first merge applies");
    Expect(Merge(published, pending, shadow, rowBytes, height, second), "second merge applies");
    Expect(pending == (Box{0, 0, 8, 6}), "the pending box covers both merges");
    Expect(UploadBox(false, pending, width, height) == pending, "an existing texture uploads only the pending box");
    bool bothCopied = true;
    for (const Box& b : {first, second})
        for (std::uint32_t y = b.top; y < b.bottom; ++y)
            for (std::uint32_t x = b.left * 4; x < b.right * 4; ++x)
                if (published[static_cast<std::size_t>(y) * rowBytes + x] != src[static_cast<std::size_t>(y) * rowBytes + x])
                    bothCopied = false;
    Expect(bothCopied, "both merged areas hold the shadow's pixels");

    Expect(UploadBox(true, pending, width, height) == Full(width, height), "a fresh texture uploads the whole frame");
    Expect(UploadBox(false, Box{}, width, height) == Full(width, height), "nothing pending uploads the whole frame");

    std::vector<std::uint8_t> wrongSize(src.size() - 4, 0);
    Box untouched{1, 1, 2, 2};
    Expect(!Merge(wrongSize, untouched, shadow, rowBytes, height, first), "a layout mismatch refuses to merge");
    Expect(untouched == (Box{1, 1, 2, 2}), "a refused merge leaves the pending box alone");

    if (failures) return 1;
    std::printf("partial upload policy tests passed\n");
    return 0;
}
