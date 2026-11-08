#include "PrismaUI/PointerMoveCoalescer.h"

#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <vector>

using PrismaUI::PointerMoveCoalescer::Coalescer;

static int failures = 0;

static void expect(bool condition, const char* message) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}

struct Move {
    int x = 0;
};

struct Owner {
    Coalescer<Move> coalescer;
    std::deque<std::function<void()>> queue;
    std::vector<std::string> delivered;

    void move(std::uint64_t view, int x) {
        const auto ticket = coalescer.Submit(view, Move{x});
        if (!ticket) return;
        queue.push_back([this, view, ticket] {
            delivered.push_back("move" + std::to_string(view) + ":" + std::to_string(coalescer.Take(view, ticket).x));
        });
    }

    void button(std::uint64_t view, const char* name) {
        coalescer.Barrier(view);
        queue.push_back([this, view, name] { delivered.push_back(std::string(name) + std::to_string(view)); });
    }

    void drain() {
        while (!queue.empty()) {
            auto task = std::move(queue.front());
            queue.pop_front();
            task();
        }
    }
};

static void testBurstFoldsIntoOneTask() {
    Owner owner;
    for (int x = 1; x <= 10000; ++x) owner.move(1, x);
    expect(owner.queue.size() == 1, "a burst of moves on one view keeps one queued task");
    owner.drain();
    expect(owner.delivered == std::vector<std::string>{"move1:10000"}, "the folded task delivers the latest position");
}

static void testClickKeepsOrder() {
    Owner owner;
    owner.move(1, 10);
    owner.move(1, 20);
    owner.button(1, "down");
    owner.move(1, 30);
    owner.move(1, 40);
    owner.button(1, "up");
    owner.drain();
    const std::vector<std::string> want{"move1:20", "down1", "move1:40", "up1"};
    expect(owner.delivered == want, "a move never crosses a button event and no button event is dropped");
}

static void testViewsAreIndependent() {
    Owner owner;
    owner.move(1, 1);
    owner.move(2, 2);
    owner.move(1, 3);
    owner.button(2, "down");
    owner.move(1, 4);
    owner.drain();
    const std::vector<std::string> want{"move1:4", "move2:2", "down2"};
    expect(owner.delivered == want, "a barrier on one view does not split moves queued for another view");
}

static void testNextMoveAfterDeliveryQueuesAgain() {
    Owner owner;
    owner.move(1, 1);
    owner.drain();
    owner.move(1, 2);
    expect(owner.queue.size() == 1, "a move after the previous one was delivered queues a new task");
    owner.drain();
    expect(owner.delivered.size() == 2 && owner.delivered[1] == "move1:2", "the later move is delivered");
}

static void testRejectedEnqueueDoesNotFreezePointer() {
    Owner owner;
    const auto ticket = owner.coalescer.Submit(1, Move{5});
    expect(static_cast<bool>(ticket), "first move yields a ticket to enqueue");
    owner.coalescer.Abandon(1, ticket);
    owner.move(1, 6);
    expect(owner.queue.size() == 1, "after an abandoned ticket the next move queues instead of folding into nothing");
    owner.drain();
    expect(owner.delivered == std::vector<std::string>{"move1:6"}, "the next move is delivered");
}

static void testStaleTakeDoesNotCloseNewerTicket() {
    Coalescer<Move> coalescer;
    const auto stale = coalescer.Submit(1, Move{1});
    coalescer.Barrier(1);
    const auto newer = coalescer.Submit(1, Move{2});
    expect(static_cast<bool>(stale) && static_cast<bool>(newer) && stale != newer, "a barrier makes the next move open a new ticket");
    expect(coalescer.Take(1, stale).x == 1, "a stale ticket still delivers its own position");
    expect(!coalescer.Submit(1, Move{3}), "the newer ticket stays open after the stale one is taken");
    expect(coalescer.Take(1, newer).x == 3, "the newer ticket delivers the latest folded position");
    expect(static_cast<bool>(coalescer.Submit(1, Move{4})), "the view opens a fresh ticket once the newer one is taken");
}

int main() {
    testBurstFoldsIntoOneTask();
    testClickKeepsOrder();
    testViewsAreIndependent();
    testNextMoveAfterDeliveryQueuesAgain();
    testRejectedEnqueueDoesNotFreezePointer();
    testStaleTakeDoesNotCloseNewerTicket();
    if (failures == 0) {
        std::puts("All pointer move coalescer assertions passed.");
        return 0;
    }
    std::printf("%d pointer move coalescer assertion(s) failed.\n", failures);
    return 1;
}
