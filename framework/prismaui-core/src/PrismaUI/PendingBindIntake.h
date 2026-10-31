#pragma once

#include <functional>
#include <utility>

namespace PrismaUI::PendingBindIntake {

using Task = std::function<void()>;
using Push = std::function<bool(Task)>;

// Pending-bind registration and cancellation must share one ordered game-task stream.
class Intake
{
public:
	explicit Intake(Push a_push) : push_(std::move(a_push)) {}

	[[nodiscard]] bool Bind(Task a_register) { return Submit(std::move(a_register)); }

	[[nodiscard]] bool Cancel(Task a_cancel) { return Submit(std::move(a_cancel)); }

private:
	[[nodiscard]] bool Submit(Task a_task)
	{
		if (!push_ || !a_task) return false;
		return push_(std::move(a_task));
	}

	Push push_;
};

}
