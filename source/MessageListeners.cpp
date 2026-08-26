#include "Diagnostics.h"
#include "Settings.h"
#include "UI.h"
#include "utils/Logger.h"

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	if (!a_msg)
	{
		return;
	}

	switch (a_msg->type)
	{
	case SKSE::MessagingInterface::kPostLoad:
		// DevBenchAPI's own contract: the interface can only be requested once SKSE has sent
		// kPostLoad, since that's the earliest point every plugin (DevBench included) has had
		// its own SKSEPluginLoad run.
		logger::debug("kPostLoad received; registering live diagnostics with DevBench if present");
		diagnostics::Init();
		break;

	case SKSE::MessagingInterface::kPostPostLoad:
		// By kPostPostLoad every plugin has finished its own post-load work, so SKSE Menu
		// Framework's module is guaranteed to be in the process if it is installed at all.
		logger::debug("kPostPostLoad received; registering settings page with SKSE Menu Framework");
		UI::Register();

		// Rule-17 retry: devbench's own server can still be finishing startup a moment after
		// kPostLoad, which is early enough to lose the race even though kPostLoad is
		// DevBenchAPI's own documented earliest-safe point (see AutoDraw-SMF's own
		// MessageListeners.cpp, where this was confirmed from a real launch's timestamps).
		// Cheap no-op if the kPostLoad attempt already succeeded.
		diagnostics::Init();
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		// Everything this mod needs (the skill perk trees, the player character) is available
		// by kDataLoaded, so there is nothing else to wire up here beyond the last DevBench
		// retry - unlike AutoDraw-SMF, this mod has no combat/animation event sink to attach.
		logger::debug("kDataLoaded received");

		// Last retry point - if DevBench still isn't found here, conclude it isn't installed
		// and say so, rather than staying silent about it forever.
		diagnostics::Init(/* a_lastAttempt = */ true);
		break;

	default:
		break;
	}
}
