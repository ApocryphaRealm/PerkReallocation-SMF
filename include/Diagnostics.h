#pragma once

// Backs the "perkreallocation.status" DevBench tool - see CLAUDE.md rule 31 (every mod's
// first version ships with live-queryable state, not just logs written after the fact). This
// matters especially here: a wrong perk removal is not something you want to only discover
// from a log after the fact, so being able to query "what did the last respec actually do"
// live is worth the small amount of extra code.
//
// Every Record* function here is called from the main thread, at the exact point a decision
// is made, and only ever writes a mutex-guarded snapshot. The DevBench tool handler runs on
// devbench's own thread and only ever reads that snapshot - it never reaches back into game
// state from it.
namespace diagnostics
{
	// Looks up the DevBench interface (present only if the DevBench plugin is installed) and
	// registers "perkreallocation.status". Safe to call repeatedly - see AutoDraw-SMF's own
	// Diagnostics.h for why this is a rule-17 retry rather than a one-shot lookup: call it
	// again at kPostPostLoad and kDataLoaded too. Every call after the first successful one is
	// a cheap no-op; only the final call (a_lastAttempt = true) logs that DevBench was never
	// found, so the "not installed" conclusion is not reported before every retry is exhausted.
	void Init(bool a_lastAttempt = false);

	// A respec actually happened - one call per successful Respec::PerformRespec().
	void RecordRespec(std::string_view a_scopeName, std::uint32_t a_perksOwnedInScope,
		std::uint32_t a_pointsRefunded, std::uint32_t a_goldCharged);

	// A respec was requested but refused (nothing owned in scope, not enough gold, or the
	// once-per-day limit) - a_reason is the same message the settings page shows.
	void RecordRespecRefused(std::string_view a_reason);
}
