from pathlib import Path

root = Path(__file__).resolve().parents[1]
text = (root / "src" / "PrismaUI" / "InputMask.cpp").read_text(encoding="utf-8")

assert "kUserEventFlags" in text and "for (const auto flag : kUserEventFlags)" in text
assert "kOtherEventFlags" in text and "for (const auto flag : kOtherEventFlags)" in text
assert "EnableUserEvent(layerId, flag, enable, senderId)" in text
assert "EnableOtherEvent(layerId, flag, enable, senderId)" in text
assert "EnableUserEvent(layerId, static_cast<UEF>(kUserEventMask)" not in text
assert "EnableOtherEvent(layerId, static_cast<OEF>(kOtherEventMask)" not in text

assert "if (!result.ok())" in text
assert "rolling back the Prisma input layer" in text

assert "userStillEnabled" in text and "otherStillEnabled" in text
assert "aggregate cached flags may remain" in text
assert "bits NOT all reflected" not in text

print("input mask per-flag routing guard passed")
