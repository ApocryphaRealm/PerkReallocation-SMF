#include "Respec.h"

#include "Diagnostics.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cmath>

namespace Respec
{
	namespace
	{
		constexpr std::uint32_t kSkillCount = 18;

		// -1.0 means "no respec has happened yet this session" - Calendar::GetDaysPassed()
		// is always >= 0, so it can never collide with a real value.
		float lastRespecDaysPassed = -1.0F;

		RE::ActorValue SkillActorValueFromScopeIndex(std::uint32_t a_oneBasedSkillIndex)
		{
			// kScopeNames[1] ("One-Handed") maps to RE::ActorValue::kOneHanded, kScopeNames[2]
			// to kOneHanded+1, and so on - see CLAUDE.md rule 30 (verified directly against the
			// vendored RE/A/ActorValues.h enum, not inferred from a table found online): the 18
			// skills are exactly kOneHanded (6) through kEnchanting (23), contiguous.
			return static_cast<RE::ActorValue>(
				static_cast<std::uint32_t>(RE::ActorValue::kOneHanded) + (a_oneBasedSkillIndex - 1));
		}

		// Depth-first walk of one skill's perk tree, collecting every rank of every perk in it.
		// Ported from the DFS-with-a-visited-list shape in covey-j/ActorCopyLib's
		// PerkUtil.cpp::DFS - a node can be reached through more than one parent in these trees,
		// so a plain recursive walk without a visited guard would revisit (and double-count)
		// shared nodes.
		void CollectNodePerks(RE::BGSSkillPerkTreeNode* a_node, std::vector<RE::BGSSkillPerkTreeNode*>& a_visited,
			std::vector<RE::BGSPerk*>& a_out)
		{
			if (!a_node || std::ranges::find(a_visited, a_node) != a_visited.end())
			{
				return;
			}

			a_visited.push_back(a_node);

			// Every rank of a multi-rank perk (e.g. Smithing's five tiers) is its own BGSPerk,
			// chained through nextPerk - each one is a separate point the player spent.
			for (RE::BGSPerk* rank = a_node->perk; rank; rank = rank->nextPerk)
			{
				a_out.push_back(rank);
			}

			for (RE::BGSSkillPerkTreeNode* child : a_node->children)
			{
				CollectNodePerks(child, a_visited, a_out);
			}
		}

		std::vector<RE::BGSPerk*> CollectSkillPerks(RE::ActorValue a_skill)
		{
			std::vector<RE::BGSPerk*> perks;

			RE::ActorValueList* avList = RE::ActorValueList::GetSingleton();

			if (!avList)
			{
				logger::error("CollectSkillPerks: RE::ActorValueList::GetSingleton() returned null");

				return perks;
			}

			RE::ActorValueInfo* avInfo = avList->GetActorValue(a_skill);

			if (!avInfo)
			{
				logger::error("CollectSkillPerks: no ActorValueInfo for actor value {}",
					static_cast<std::uint32_t>(a_skill));

				return perks;
			}

			if (!avInfo->perkTree)
			{
				logger::debug("CollectSkillPerks: \"{}\" has no perk tree; nothing to collect", avInfo->enumName);

				return perks;
			}

			std::vector<RE::BGSSkillPerkTreeNode*> visited;
			CollectNodePerks(avInfo->perkTree, visited, perks);

			logger::trace("CollectSkillPerks: {} perk rank(s) found in \"{}\"'s tree", perks.size(), avInfo->enumName);

			return perks;
		}

		// Every skill's perks when a_scopeIndex is 0 ("All Skills"), or just the one skill's
		// perks otherwise.
		std::vector<RE::BGSPerk*> CollectPerksInScope(std::uint32_t a_scopeIndex)
		{
			std::vector<RE::BGSPerk*> perks;

			if (a_scopeIndex == 0)
			{
				for (std::uint32_t i = 1; i <= kSkillCount; ++i)
				{
					std::vector<RE::BGSPerk*> skillPerks = CollectSkillPerks(SkillActorValueFromScopeIndex(i));
					perks.insert(perks.end(), skillPerks.begin(), skillPerks.end());
				}
			}
			else
			{
				perks = CollectSkillPerks(SkillActorValueFromScopeIndex(a_scopeIndex));
			}

			return perks;
		}
	}

	Result PerformRespec(std::uint32_t a_scopeIndex)
	{
		Result result;

		if (a_scopeIndex >= static_cast<std::uint32_t>(kScopeCount))
		{
			result.message = "Invalid respec scope selected.";
			logger::error("PerformRespec: scope index {} is out of range (0-{})", a_scopeIndex, kScopeCount - 1);
			diagnostics::RecordRespecRefused(result.message);

			return result;
		}

		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

		if (!player)
		{
			result.message = "Could not find the player character.";
			logger::error("PerformRespec: RE::PlayerCharacter::GetSingleton() returned null");
			diagnostics::RecordRespecRefused(result.message);

			return result;
		}

		RE::Calendar* calendar = RE::Calendar::GetSingleton();
		const float currentDaysPassed = calendar ? calendar->GetDaysPassed() : -1.0F;

		if (!calendar)
		{
			// Not fatal - the once-per-day limit just cannot be enforced this call, which is
			// safer than refusing every respec because the calendar could not be reached.
			logger::warn("PerformRespec: RE::Calendar::GetSingleton() returned null; "
						 "the once-per-day limit cannot be checked this time");
		}

		if (settings::respec::limitOncePerDay && calendar && lastRespecDaysPassed >= 0.0F &&
			std::floor(currentDaysPassed) == std::floor(lastRespecDaysPassed))
		{
			result.message = "Already respecced today. Try again after the in-game day changes.";
			logger::debug("PerformRespec: refused - already respecced this in-game day ({:.2f})", currentDaysPassed);
			diagnostics::RecordRespecRefused(result.message);

			return result;
		}

		const std::vector<RE::BGSPerk*> perksInScope = CollectPerksInScope(a_scopeIndex);

		std::vector<RE::BGSPerk*> ownedPerks;
		ownedPerks.reserve(perksInScope.size());

		for (RE::BGSPerk* perk : perksInScope)
		{
			if (perk && player->HasPerk(perk))
			{
				ownedPerks.push_back(perk);
			}
		}

		result.perksOwnedInScope = static_cast<std::uint32_t>(ownedPerks.size());

		logger::debug("PerformRespec: scope \"{}\" - {} perk(s) found, {} owned by the player",
			kScopeNames[a_scopeIndex], perksInScope.size(), ownedPerks.size());

		if (ownedPerks.empty())
		{
			result.success = true;
			result.message = std::format("No spent perks found in \"{}\" - nothing to refund.", kScopeNames[a_scopeIndex]);

			return result;
		}

		const float refundFraction = std::clamp(settings::respec::refundPercent, 0.0F, 100.0F) / 100.0F;
		const std::uint32_t refundPoints = static_cast<std::uint32_t>(
			std::lround(static_cast<float>(ownedPerks.size()) * refundFraction));

		const std::uint32_t goldCost = static_cast<std::uint32_t>(
			std::lround(static_cast<float>(refundPoints) * std::max(0.0F, settings::respec::goldCostPerPoint)));

		if (goldCost > 0)
		{
			const std::int32_t playerGold = player->GetGoldAmount();

			if (playerGold < 0 || static_cast<std::uint32_t>(playerGold) < goldCost)
			{
				result.message = std::format("Not enough gold - this respec costs {}, you have {}.",
					goldCost, std::max(playerGold, 0));
				logger::debug("PerformRespec: refused - needs {} gold, player has {}", goldCost, playerGold);
				diagnostics::RecordRespecRefused(result.message);

				return result;
			}

			RE::TESBoundObject* goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);  // Gold001

			if (!goldForm)
			{
				result.message = "Could not find the gold item; respec cancelled without charging or refunding anything.";
				logger::error("PerformRespec: TESForm::LookupByID(0xF) (Gold001) returned null");
				diagnostics::RecordRespecRefused(result.message);

				return result;
			}

			player->RemoveItem(goldForm, static_cast<std::int32_t>(goldCost),
				RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

			result.goldCharged = goldCost;
			logger::debug("PerformRespec: charged {} gold", goldCost);
		}

		for (RE::BGSPerk* perk : ownedPerks)
		{
			player->RemovePerk(perk);
			logger::trace("PerformRespec: removed perk \"{}\"", perk->GetFullName());
		}

		// perkCount is a signed 8-bit field behind PlayerCharacter's version-resolved runtime
		// data accessor (RE/P/PlayerCharacter.h's GAME_STATE_DATA_CONTENT, reached through
		// GetPlayerRuntimeData() rather than as a direct member - confirmed against this
		// project's own vendored header, not assumed). It is the same field skse64's original
		// papyrusGame::ModPerkPoints and ActorCopyLib's PlayerUtil::ModPerkPoints both
		// read/write under slightly different names (numPerkPoints / perkCount) - clamp rather
		// than let it wrap negative.
		auto& runtimeData = player->GetPlayerRuntimeData();
		const int newPerkCount = std::clamp(static_cast<int>(runtimeData.perkCount) + static_cast<int>(refundPoints), 0, 127);

		if (newPerkCount != static_cast<int>(runtimeData.perkCount) + static_cast<int>(refundPoints))
		{
			logger::warn("PerformRespec: perkCount would have exceeded the field's 127 maximum; clamped");
		}

		runtimeData.perkCount = static_cast<std::int8_t>(newPerkCount);

		lastRespecDaysPassed = currentDaysPassed;

		result.success = true;
		result.pointsRefunded = refundPoints;
		result.message = std::format(
			"Refunded {} of {} spent perk point(s) from \"{}\"{}.",
			refundPoints, ownedPerks.size(), kScopeNames[a_scopeIndex],
			goldCost > 0 ? std::format(" (charged {} gold)", goldCost) : std::string{});

		logger::info("PerformRespec: {}", result.message);

		diagnostics::RecordRespec(kScopeNames[a_scopeIndex], result.perksOwnedInScope, result.pointsRefunded, result.goldCharged);

		return result;
	}
}
