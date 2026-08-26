#pragma once

namespace SKSE::log
{
	using level = spdlog::level::level_enum;
}
namespace logger = SKSE::log;

namespace settings
{
	// Reads the INI into the variables below. The values the variables hold when this is
	// called are remembered as the built-in defaults, so RestoreDefaults() can put them back.
	void Init(const std::string& a_iniFileName);

	// Writes every setting below back to the INI that Init() read, leaving the comments and
	// any unrelated keys in that file alone. Returns false if the file could not be written.
	bool Save();

	// Puts every setting back to its built-in default. This only touches the variables;
	// follow it with Save() to persist, and with UI::ApplyLiveSettings() to show it in game.
	void RestoreDefaults();

	// Re-reads the INI that Init() read, discarding any unsaved change made since. Returns
	// false if the file could not be read, leaving the current values alone.
	bool Reload();

	// Full path of the INI Init() read, or an empty string before Init() has run.
	const std::string& GetIniPath();

	namespace debug
	{
		// Ships at trace by default (project standard) so a submitted log carries the detail
		// needed to diagnose a compatibility, timing or stability report without asking the
		// reporter to change anything first - see CLAUDE.md rule 31.
		inline logger::level logLevel = logger::level::trace;
	}

	namespace respec
	{
		// Index into UI's kScopeNames: 0 = "All Skills" (every one of the 18 skill trees),
		// 1..18 = one specific skill tree (1 = One-Handed .. 18 = Enchanting, matching
		// RE::ActorValue::kOneHanded + (index - 1)). Kept as a plain index rather than an
		// ActorValue directly so it round-trips through the INI as a small integer.
		inline std::uint32_t scopeIndex = 0;

		// What fraction of the perk points removed are actually handed back, 0-100. The
		// remainder is the "cost" of respeccing - Ish's original potion was a flat 100% (no
		// cost at all beyond finding the potion), which this fork deliberately makes
		// configurable rather than copying unchanged. Defaults to 100 (matches the original's
		// behaviour out of the box; the trade-off is opt-in).
		inline float refundPercent = 100.0F;

		// Gold charged per perk point actually refunded (not per point lost to the refund
		// percentage above). 0 disables the cost entirely. This is the "genuine cost" option
		// the task asked to consider - a simple, always-available economy sink rather than a
		// dragon-soul cost (that's Ish's Souls to Perks' own mechanic, a different queued mod -
		// see CLAUDE.md; this fork's own reasonable design is a plain gold cost instead).
		inline float goldCostPerPoint = 0.0F;

		// When on, only one respec (regardless of scope) is allowed per in-game calendar day,
		// tracked in memory only for this first version - see Respec.cpp's own comment on why
		// that is a known, deliberate limitation rather than an oversight.
		inline bool limitOncePerDay = false;
	}
}
