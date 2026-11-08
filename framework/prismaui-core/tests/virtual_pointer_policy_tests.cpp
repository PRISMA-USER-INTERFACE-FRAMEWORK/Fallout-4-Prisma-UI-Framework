#include "PrismaUI/VirtualPointerPolicy.h"

#include <cassert>
#include <cmath>

using namespace PrismaUI::VirtualPointerPolicy;

static bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0005F;
}

static void TestShapeContract()
{
    assert(Near(Shape(0.0F), 0.0F));
    assert(Near(Shape(0.1F), 0.1F));
    assert(Near(Shape(-0.1F), -0.1F));
    assert(Near(Shape(1.0F), 1.0F));
    assert(Near(Shape(-1.0F), -1.0F));
    assert(Near(Shape(1.5F), 1.0F));
    assert(Near(Shape(-4.0F), -1.0F));
    assert(Near(Shape(NAN), 0.0F));
}

static void TestShapeIsMonotonic()
{
    float previous = 0.0F;
    for (float value = 0.0F; value <= 1.0F; value += 0.01F) {
        const float shaped = Shape(value);
        assert(shaped >= previous);
        previous = shaped;
    }
    assert(Near(previous, 1.0F));
}

static void TestDeltaClamp()
{
    assert(Near(ClampDelta(-1.0F), 0.0F));
    assert(Near(ClampDelta(0.0F), 0.0F));
    assert(Near(ClampDelta(0.01F), 0.01F));
    assert(Near(ClampDelta(kMaxDeltaSeconds), kMaxDeltaSeconds));
    assert(Near(ClampDelta(10.0F), kMaxDeltaSeconds));
}

static void TestSmallSticksMove()
{
    assert(Step(500, 0.1F, kMaxDeltaSeconds, 0, 1000) > 500);
    assert(Step(500, -0.1F, kMaxDeltaSeconds, 0, 1000) < 500);
    assert(Step(500, 0.0F, 1.0F, 0, 1000) == 500);
}

static void TestFullDeflectionSpeed()
{
    const int expected = static_cast<int>(kSpeedPixelsPerSecond * kMaxDeltaSeconds + 0.5F);
    assert(Step(500, 1.0F, kMaxDeltaSeconds, 0, 1000) == 500 + expected);
    assert(Step(500, -1.0F, kMaxDeltaSeconds, 0, 1000) == 500 - expected);
}

static void TestStallCannotTeleport()
{
    assert(Step(500, 1.0F, 60.0F, 0, 1000) == Step(500, 1.0F, kMaxDeltaSeconds, 0, 1000));
    assert(Step(500, 1.0F, -5.0F, 0, 1000) == 500);
}

static void TestClampingAtEdges()
{
    assert(Step(990, 1.0F, kMaxDeltaSeconds, 10, 1000) == 1000);
    assert(Step(20, -1.0F, kMaxDeltaSeconds, 10, 1000) == 10);
    assert(Step(1000, 1.0F, kMaxDeltaSeconds, 0, 1000) == 1000);
    assert(Step(1, 1.0F, kMaxDeltaSeconds, 0, 1000) == 1 + static_cast<int>(kSpeedPixelsPerSecond * kMaxDeltaSeconds + 0.5F));
}

static void TestSnapsInsideFromOutside()
{
    assert(Step(9, 1.0F, kMaxDeltaSeconds, 10, 1000) == 10);
    assert(Step(-4000, 1.0F, kMaxDeltaSeconds, 10, 1000) == 10);
    assert(Step(5000, -1.0F, kMaxDeltaSeconds, 10, 1000) == 1000);
    assert(Step(9, 0.0F, kMaxDeltaSeconds, 10, 1000) == 10);
}

static void TestDegenerateRect()
{
    assert(Step(500, 1.0F, kMaxDeltaSeconds, 900, 100) == 900);
    assert(Step(500, 1.0F, kMaxDeltaSeconds, 7, 7) == 7);
}

static void TestHalfDeflectionIsHalfTravel()
{
    const float half = 0.5F;
    const int full = Step(0, 1.0F, kMaxDeltaSeconds, -100000, 100000);
    const int partial = Step(0, half, kMaxDeltaSeconds, -100000, 100000);
    assert(partial > 0);
    assert(partial < full);
}

static void TestStickDirections()
{
    assert(StepX(500, 1.0F, kMaxDeltaSeconds, 0, 1000) > 500);
    assert(StepX(500, -1.0F, kMaxDeltaSeconds, 0, 1000) < 500);
    assert(StepY(500, 1.0F, kMaxDeltaSeconds, 0, 1000) < 500);
    assert(StepY(500, -1.0F, kMaxDeltaSeconds, 0, 1000) > 500);
    assert(StepX(500, 0.0F, kMaxDeltaSeconds, 0, 1000) == 500);
    assert(StepY(500, 0.0F, kMaxDeltaSeconds, 0, 1000) == 500);
}

static void TestStickDirectionsRespectEdges()
{
    assert(StepX(1000, 1.0F, kMaxDeltaSeconds, 0, 1000) == 1000);
    assert(StepX(0, -1.0F, kMaxDeltaSeconds, 0, 1000) == 0);
    assert(StepY(0, 1.0F, kMaxDeltaSeconds, 0, 1000) == 0);
    assert(StepY(1000, -1.0F, kMaxDeltaSeconds, 0, 1000) == 1000);
}

int main()
{
    TestShapeContract();
    TestShapeIsMonotonic();
    TestDeltaClamp();
    TestSmallSticksMove();
    TestFullDeflectionSpeed();
    TestStallCannotTeleport();
    TestClampingAtEdges();
    TestSnapsInsideFromOutside();
    TestDegenerateRect();
    TestHalfDeflectionIsHalfTravel();
    TestStickDirections();
    TestStickDirectionsRespectEdges();
    return 0;
}
