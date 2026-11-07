#pragma once

// Pending binds must classify availability before retrying; source:docs/code-notes/prismaui-core.md.
#include <chrono>
#include <cstdint>

namespace PrismaUI::MeshBinding::Readiness
{
	enum class Availability
	{
		Available,
		DisabledByConfig,
		UnsupportedRuntime
	};

	[[nodiscard]] constexpr Availability ResolveAvailability(bool a_configEnabled, bool a_layoutVerified) noexcept
	{
		if (!a_configEnabled) return Availability::DisabledByConfig;
		if (!a_layoutVerified) return Availability::UnsupportedRuntime;
		return Availability::Available;
	}

	[[nodiscard]] constexpr const char* AvailabilityReason(Availability a_availability) noexcept
	{
		switch (a_availability) {
		case Availability::Available:
			return "available";
		case Availability::DisabledByConfig:
			return "disabled by [Debug] bMeshBinding";
		case Availability::UnsupportedRuntime:
			return "no verified engine texture layout for this runtime";
		}
		return "unavailable";
	}

	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::milliseconds;

	struct Policy
	{
		Milliseconds timeout{ 2000 };
		Milliseconds interval{ 100 };
		std::uint32_t maxAttempts{ 15 };
	};

	struct State
	{
		Clock::time_point started{};
		Clock::time_point nextAttempt{};
		std::uint32_t attempts{};
	};

	enum class Decision
	{
		AttemptNow,
		Wait,
		Expired,
		ViewGone,
		ReferenceGone,
		Unavailable
	};

	[[nodiscard]] constexpr State Begin(Clock::time_point a_now) noexcept
	{
		return State{ a_now, a_now, 0 };
	}

	[[nodiscard]] constexpr Milliseconds Elapsed(Clock::time_point a_now, Clock::time_point a_started) noexcept
	{
		return std::chrono::duration_cast<Milliseconds>(a_now - a_started);
	}

	[[nodiscard]] constexpr bool IsDue(const State& a_state, Clock::time_point a_now) noexcept
	{
		return a_now >= a_state.nextAttempt;
	}

	// A stale view or reference must cancel immediately; only a live bind waits for readiness.
	[[nodiscard]] constexpr Decision Classify(const State& a_state, Clock::time_point a_now, const Policy& a_policy,
	                                          Availability a_availability, bool a_viewLive,
	                                          bool a_referenceLive) noexcept
	{
		if (a_availability != Availability::Available) return Decision::Unavailable;
		if (!a_viewLive) return Decision::ViewGone;
		if (!a_referenceLive) return Decision::ReferenceGone;
		if (a_state.attempts >= a_policy.maxAttempts) return Decision::Expired;
		if (Elapsed(a_now, a_state.started) >= a_policy.timeout) return Decision::Expired;
		return IsDue(a_state, a_now) ? Decision::AttemptNow : Decision::Wait;
	}

	// Server-side ordering: record the attempt and its next due time under one state update.
	inline void NoteAttempt(State& a_state, Clock::time_point a_now, const Policy& a_policy) noexcept
	{
		++a_state.attempts;
		a_state.nextAttempt = a_now + a_policy.interval;
	}
}
