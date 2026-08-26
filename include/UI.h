#pragma once

namespace UI
{
	// Adds this mod's page to the SKSE Menu Framework's Mod Control Panel. Safe to call when
	// the framework is missing or too old to drive: it logs why and does nothing else.
	void Register();

	namespace SettingsPanel
	{
		void __stdcall Render();
	}
}
