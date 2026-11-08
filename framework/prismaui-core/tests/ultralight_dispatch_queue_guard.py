from pathlib import Path


QUEUE_HEADER = Path("src/PrismaUI/UltralightDispatchQueue.h").read_text(encoding="utf-8")
QUEUE_SOURCE = Path("src/PrismaUI/UltralightDispatchQueue.cpp").read_text(encoding="utf-8")
BACKEND_DISPATCH = Path("src/PrismaUI/UltralightBackendDispatch.cpp").read_text(encoding="utf-8")
VIEW_MANAGER = Path("src/PrismaUI/UltralightViewManager.cpp").read_text(encoding="utf-8")

assert "std::queue<Entry> tasks_;" in QUEUE_HEADER
assert "std::condition_variable condition_;" in QUEUE_HEADER
assert "std::atomic_bool stopping_{false};" in QUEUE_HEADER
assert "std::atomic_bool frameRequested_{false};" in QUEUE_HEADER
assert "if (IsOwnerThread())" in QUEUE_SOURCE
assert "task();" in QUEUE_SOURCE
assert "if (stopping_.load(std::memory_order_acquire)) return false;" in QUEUE_SOURCE
assert "condition_.wait(lock" in QUEUE_SOURCE
assert "frameRequested_.exchange(false" in QUEUE_SOURCE
assert "viewManager_.DispatchState(id)" in BACKEND_DISPATCH
assert "ViewDispatchState::Rejected" in BACKEND_DISPATCH
assert "ViewDispatchState::Queued" in BACKEND_DISPATCH
assert "ViewDispatchState::Unknown" in BACKEND_DISPATCH
assert "ViewReservationState::DestroyRequested" in VIEW_MANAGER
assert "ViewReservationState::Reserved" in VIEW_MANAGER
assert "ViewReservationState::Creating" in VIEW_MANAGER
assert "std::queue<Task>" not in BACKEND_DISPATCH

print("Ultralight dispatch queue boundary guard passed")
