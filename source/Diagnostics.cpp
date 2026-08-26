#include "Diagnostics.h"

#include "DevBench/DevBenchAPI.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <mutex>

namespace diagnostics
{
	namespace
	{
		using clock = std::chrono::steady_clock;

		std::mutex mtx;

		struct State
		{
			std::uint64_t respecsApplied = 0;
			std::optional<clock::time_point> lastRespec;
			std::string lastRespecScope;
			std::uint32_t lastPerksOwned = 0;
			std::uint32_t lastPointsRefunded = 0;
			std::uint32_t lastGoldCharged = 0;

			std::uint64_t respecsRefused = 0;
			std::optional<clock::time_point> lastRefusal;
			std::string lastRefusalReason;
		};

		State state;

		std::string EscapeJson(std::string_view a_text)
		{
			std::string out;
			out.reserve(a_text.size());

			for (char c : a_text)
			{
				switch (c)
				{
				case '"':
					out += "\\\"";
					break;
				case '\\':
					out += "\\\\";
					break;
				case '\n':
					out += "\\n";
					break;
				default:
					out += c;
					break;
				}
			}

			return out;
		}

		std::string SecondsAgoField(const char* a_name, const std::optional<clock::time_point>& a_when)
		{
			if (!a_when)
			{
				return std::format("\"{}SecondsAgo\": null", a_name);
			}

			const double seconds = std::chrono::duration<double>(clock::now() - *a_when).count();

			return std::format("\"{}SecondsAgo\": {:.1f}", a_name, seconds);
		}

		void StatusTool(void*, const char*, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			std::string json;

			{
				std::scoped_lock lock(mtx);

				json = std::format(
					"{{"
					"\"settings\":{{"
					"\"scopeIndex\":{},"
					"\"refundPercent\":{:.1f},"
					"\"goldCostPerPoint\":{:.1f},"
					"\"limitOncePerDay\":{}"
					"}},"
					"\"lastRespec\":{{"
					"\"count\":{},"
					"\"scope\":\"{}\","
					"\"perksOwned\":{},"
					"\"pointsRefunded\":{},"
					"\"goldCharged\":{},"
					"{}"
					"}},"
					"\"lastRefusal\":{{"
					"\"count\":{},"
					"\"reason\":\"{}\","
					"{}"
					"}}"
					"}}",
					settings::respec::scopeIndex,
					settings::respec::refundPercent,
					settings::respec::goldCostPerPoint,
					settings::respec::limitOncePerDay ? "true" : "false",
					state.respecsApplied,
					EscapeJson(state.lastRespecScope),
					state.lastPerksOwned,
					state.lastPointsRefunded,
					state.lastGoldCharged,
					SecondsAgoField("last", state.lastRespec),
					state.respecsRefused,
					EscapeJson(state.lastRefusalReason),
					SecondsAgoField("last", state.lastRefusal));
			}

			a_write(a_sink, json.c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;

		if (registered)
		{
			return;
		}

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();

		if (!devBench)
		{
			if (a_lastAttempt)
			{
				logger::info("DevBench not detected; skipping the \"perkreallocation.status\" live-diagnostics "
							 "tool (logging alone still covers this session - see CLAUDE.md rule 31)");
			}
			else
			{
				logger::debug("DevBench not detected yet; will retry at the next message");
			}

			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Live Perk Reallocation state: current settings and what the last "
			"respec attempt (applied or refused) actually did.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{}},"
			"\"readOnly\":true"
			"}";

		if (devBench->RegisterTool("perkreallocation.status", descriptor, &StatusTool, nullptr))
		{
			logger::info("Registered \"perkreallocation.status\" with DevBench (build {})", devBench->GetBuildNumber());
		}
		else
		{
			logger::warn("DevBench reported \"perkreallocation.status\" replaced an existing tool of the same name");
		}

		registered = true;
	}

	void RecordRespec(std::string_view a_scopeName, std::uint32_t a_perksOwnedInScope,
		std::uint32_t a_pointsRefunded, std::uint32_t a_goldCharged)
	{
		std::scoped_lock lock(mtx);

		++state.respecsApplied;
		state.lastRespec = clock::now();
		state.lastRespecScope = a_scopeName;
		state.lastPerksOwned = a_perksOwnedInScope;
		state.lastPointsRefunded = a_pointsRefunded;
		state.lastGoldCharged = a_goldCharged;
	}

	void RecordRespecRefused(std::string_view a_reason)
	{
		std::scoped_lock lock(mtx);

		++state.respecsRefused;
		state.lastRefusal = clock::now();
		state.lastRefusalReason = a_reason;
	}
}
