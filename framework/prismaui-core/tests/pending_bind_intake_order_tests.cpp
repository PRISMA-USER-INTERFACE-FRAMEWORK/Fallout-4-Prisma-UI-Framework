#include "PrismaUI/PendingBindIntake.h"

#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>

using PrismaUI::PendingBindIntake::Intake;
using PrismaUI::PendingBindIntake::Task;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

struct FakeGameThread {
    std::deque<Task> queue;
    std::map<std::string, std::string> bindings;
    std::set<std::string> views;

    Intake MakeIntake()
    {
        return Intake([this](Task task) {
            queue.push_back(std::move(task));
            return true;
        });
    }

    void Drain()
    {
        while (!queue.empty()) {
            Task task = std::move(queue.front());
            queue.pop_front();
            task();
        }
    }
};

int main()
{
    {
        FakeGameThread game;
        Intake intake = game.MakeIntake();
        game.bindings["bench"] = "OldTarget";
        game.views.insert("bench");

        expect(intake.Cancel([&game] { game.bindings.erase("bench"); }), "Unbind is accepted");
        expect(intake.Bind([&game] { game.bindings["bench"] = "NewTarget"; }), "BindToObject is accepted");
        expect(game.queue.size() == 2, "both submissions stay queued instead of running inline");
        expect(game.bindings["bench"] == "OldTarget", "the queued unbind has not run before the game task drains");

        game.Drain();
        expect(game.bindings.count("bench") == 1, "a rebind issued right after Unbind survives the drain");
        expect(game.bindings["bench"] == "NewTarget", "the surviving binding is the rebind target");
    }

    {
        FakeGameThread game;
        Intake intake = game.MakeIntake();
        game.bindings["bench"] = "OldTarget";
        game.views.insert("bench");

        expect(intake.Cancel([&game] { game.bindings.erase("bench"); game.views.erase("bench"); }),
               "Destroy is accepted");
        game.queue.push_back([&game] { game.views.insert("bench"); });
        expect(intake.Bind([&game] { game.bindings["bench"] = "NewTarget"; }), "the rebind is accepted");
        expect(game.bindings.count("bench") == 1 && game.bindings["bench"] == "OldTarget",
               "nothing ran inline ahead of the queued destroy");

        game.Drain();
        expect(game.views.count("bench") == 1, "the recreated view exists after the drain");
        expect(game.bindings.count("bench") == 1, "destroying and recreating the name keeps the new binding");
        expect(game.bindings["bench"] == "NewTarget", "the binding belongs to the recreated view");
    }

    {
        FakeGameThread game;
        Intake intake = game.MakeIntake();
        game.bindings["bench"] = "Live";
        (void)intake.Bind([&game] { game.bindings["bench"] = "Rebound"; });
        (void)intake.Cancel([&game] { game.bindings.erase("bench"); });
        game.Drain();
        expect(game.bindings.empty(), "a later Unbind still cancels an earlier Bind");
    }

    {
        bool ran = false;
        Intake rejecting([](Task) { return false; });
        expect(!rejecting.Bind([&ran] { ran = true; }), "a rejected stream reports failure for Bind");
        expect(!rejecting.Cancel([&ran] { ran = true; }), "a rejected stream reports failure for Cancel");
        expect(!ran, "a rejected submission never runs");

        Intake empty{PrismaUI::PendingBindIntake::Push{}};
        expect(!empty.Bind([&ran] { ran = true; }), "an intake without a push callback fails closed");
        expect(!empty.Cancel([&ran] { ran = true; }), "an intake without a push callback fails closed for Cancel");
    }

    if (g_failures == 0) {
        std::printf("\nAll pending-bind intake ordering assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
