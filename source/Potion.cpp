#include "Potion.h"

#include "Respec.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <format>
#include <string>

namespace Potion
{
	namespace
	{
		// The contract with PerkReallocation.esp. See Potion.h.
		constexpr const char* kPluginFileName = "PerkReallocation.esp";
		constexpr RE::FormID kPotionLocalFormID = 0x800;

		RE::TESForm* g_potionForm = nullptr;
		bool         g_sinkRegistered = false;
		bool         g_reportedMissing = false;

		// Drinking a potion in Skyrim is an EQUIP, so TESEquipEvent is what fires - there is no
		// separate "consume" event. The event carries the base object's FormID directly, which
		// makes the check a single integer comparison and keeps this cheap: it runs for every
		// equip by every actor in the game.
		class EquipSink : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			static EquipSink* GetSingleton()
			{
				static EquipSink singleton;

				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(
				const RE::TESEquipEvent*                a_event,
				RE::BSTEventSource<RE::TESEquipEvent>*  a_source) override
			{
				(void)a_source;

				if (!a_event || !g_potionForm)
				{
					return RE::BSEventNotifyControl::kContinue;
				}

				// Cheapest discriminator first - this rejects every unrelated equip in one
				// comparison, before any pointer is dereferenced.
				if (a_event->baseObject != g_potionForm->GetFormID())
				{
					return RE::BSEventNotifyControl::kContinue;
				}

				// equipped == false is the un-equip half of the event pair. A potion produces
				// only the equip, but guarding costs nothing and stops a double-respec if that
				// ever changes.
				if (!a_event->equipped)
				{
					return RE::BSEventNotifyControl::kContinue;
				}

				RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

				if (!player || !a_event->actor || a_event->actor.get() != static_cast<RE::TESObjectREFR*>(player))
				{
					// An NPC drinking it does nothing. Perk points are the player's alone, and
					// silently respeccing the player because a merchant drank their own stock
					// would be a genuinely nasty bug.
					logger::debug("Draught of Fate Unwound consumed by a non-player actor; ignoring");

					return RE::BSEventNotifyControl::kContinue;
				}

				HandlePlayerDrink();

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			static void HandlePlayerDrink()
			{
				const std::uint32_t scope = settings::respec::scopeIndex;

				logger::info("Draught of Fate Unwound consumed by the player; respeccing scope index {}", scope);

				// Deliberately routed through the same PerformRespec the settings page calls, so
				// the potion honours the player's configured scope, refund percentage, gold cost
				// and once-per-day limit rather than reimplementing any of it. A refusal (not
				// enough gold, already respecced today) leaves the character untouched, exactly
				// as it does from the button.
				const Respec::Result result = Respec::PerformRespec(scope);

				logger::info("Potion respec result: success={} owned={} refunded={} gold={} - {}",
					result.success,
					result.perksOwnedInScope,
					result.pointsRefunded,
					result.goldCharged,
					result.message);

				// The whole point of the potion is that it works without opening a menu, so the
				// outcome has to be visible in the world rather than only in the log.
				RE::DebugNotification(result.message.c_str());
			}
		};
	}

	void Init(bool a_lastAttempt)
	{
		if (g_sinkRegistered)
		{
			return;
		}

		RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();

		if (!dataHandler)
		{
			// Not fatal and not final - rule 17 says a late-binding lookup is retried, not
			// concluded.
			logger::debug("Potion::Init: no TESDataHandler yet");

			return;
		}

		RE::TESForm* form = dataHandler->LookupForm(kPotionLocalFormID, kPluginFileName);

		if (!form)
		{
			// The supported "plugin not installed" path. Reported once, as info, on the final
			// attempt - a configuration, not a failure.
			if (a_lastAttempt && !g_reportedMissing)
			{
				g_reportedMissing = true;

				logger::info(
					"{} is not present in the load order, so the Draught of Fate Unwound is unavailable. "
					"The settings page's Respec button is unaffected.",
					kPluginFileName);
			}
			else
			{
				logger::debug("Potion::Init: {} not found yet", kPluginFileName);
			}

			return;
		}

		g_potionForm = form;

		RE::ScriptEventSourceHolder* holder = RE::ScriptEventSourceHolder::GetSingleton();

		if (!holder)
		{
			// Form resolved but nothing to listen with - leave g_sinkRegistered false so a later
			// retry can still finish the job.
			logger::warn("Potion::Init: no ScriptEventSourceHolder; the potion will not trigger a respec");

			return;
		}

		holder->AddEventSink<RE::TESEquipEvent>(EquipSink::GetSingleton());
		g_sinkRegistered = true;

		logger::info("Draught of Fate Unwound resolved from {} at 0x{:08X}; consume sink registered",
			kPluginFileName,
			form->GetFormID());
	}

	bool IsActive()
	{
		return g_sinkRegistered && g_potionForm != nullptr;
	}
}
