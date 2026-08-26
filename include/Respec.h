#pragma once

// The one genuinely new piece of this mod - see CLAUDE.md rule 24 (search before inventing).
// The perk-enumeration/removal approach here is not a guess: it follows the DFS-over-perk-tree
// pattern in covey-j/ActorCopyLib's PerkUtil.cpp/ActorValueUtil.cpp (a real, working, publicly
// available CommonLibSSE plugin found on GitHub), and the perk-point field
// (RE::PlayerCharacter::perkCount) is the same field that skse64's own original
// papyrusGame::ModPerkPoints implementation and ActorCopyLib's own PlayerUtil::ModPerkPoints
// both read/write - confirmed against this project's own vendored CommonLibSSE-NG headers
// (RE/P/PlayerCharacter.h, RE/A/ActorValueList.h, RE/A/ActorValueInfo.h, RE/B/BGSSkillPerkTreeNode.h,
// RE/B/BGSPerk.h), not invented blind. See DESIGN-NOTES.md for the full trail.
namespace Respec
{
	// Index into kScopeNames: 0 = every one of the 18 skill trees, 1..18 = one specific tree
	// (1 = One-Handed .. 18 = Enchanting). Shared between Settings, UI and Respec so there is
	// exactly one place the index-to-skill mapping is defined.
	inline constexpr const char* kScopeNames[] = {
		"All Skills",
		"One-Handed", "Two-Handed", "Archery", "Block", "Smithing", "Heavy Armor", "Light Armor",
		"Pickpocket", "Lockpicking", "Sneak", "Alchemy", "Speech", "Alteration", "Conjuration",
		"Destruction", "Illusion", "Restoration", "Enchanting"
	};
	inline constexpr int kScopeCount = 19;

	struct Result
	{
		bool success = false;

		// Every perk this respec found the player actually owns in scope, before the refund
		// percentage is applied - one BGSPerk (one rank) is one spent point.
		std::uint32_t perksOwnedInScope = 0;

		// perksOwnedInScope, after settings::respec::refundPercent is applied - this many perk
		// points are actually handed back. The rest are lost to the refund cost, same as every
		// removed perk is lost regardless of the percentage (a partial refund still clears the
		// whole scope - see DESIGN-NOTES.md for why).
		std::uint32_t pointsRefunded = 0;

		// Gold actually deducted (0 if settings::respec::goldCostPerPoint is 0).
		std::uint32_t goldCharged = 0;

		// Human-readable outcome, safe to show directly in the settings page.
		std::string message;
	};

	// Performs (or refuses) a respec for a_scopeIndex (see kScopeNames above), using the
	// current settings::respec values for refund percentage, gold cost and the once-per-day
	// limit. Safe to call from the settings page's button handler - already marshalled to the
	// main thread by the caller (see UI.cpp's OnMainThread), since this touches live actor
	// state and inventory.
	Result PerformRespec(std::uint32_t a_scopeIndex);
}
