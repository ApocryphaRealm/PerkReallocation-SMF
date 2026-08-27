#pragma once

// The "Draught of Fate Unwound" potion - the original mod's own trigger, restored.
//
// Ish's Respec Mod (Nexus 1960) worked by selling a potion through apothecaries; drinking it
// refunded every perk. This rebuild replaced that with a settings-page button and shipped no
// plugin at all, which meant the mod could only be used from a menu. The potion is now back,
// carried by PerkReallocation.esp - see PLUGIN-NOTES.md for what that plugin contains and how it
// was reproduced.
//
// HOW THIS DIFFERS FROM THE ORIGINAL, and why:
//
//   * The original's potion carried a MAGIC EFFECT WITH A PAPYRUS SCRIPT attached, and that
//     script was the entire mod - it walked the perk trees and refunded points itself. Ours
//     cannot: the respec logic is C++ in this DLL. So our plugin's effect is deliberately INERT
//     (archetype Script with no VMAD at all), and the potion exists only to be identifiable.
//     This module watches for the player consuming it and calls Respec::PerformRespec.
//
//   * The original was all-or-nothing and free. Ours honours the SETTINGS the player has set -
//     scope, refund percentage, gold cost, and the once-per-day limit. With the shipped defaults
//     the scope is "All Skills", so the potion behaves as the original did in that respect; but
//     note the gold cost now defaults to 500 per point, which the original never charged. Anyone
//     wanting the original's exact behaviour sets that to 0.
//
// THE FORM ID IS A CONTRACT with the plugin. PerkReallocation.esp declares the potion at local
// FormID 0x800; the lookup below finds it by plugin filename. Neither may drift without the
// other.
//
// The plugin is OPTIONAL at runtime. If it is absent this logs the fact once and does nothing
// further - the settings page's Respec button is unaffected, exactly as before the plugin
// existed. Losing the plugin must never lose the mod's function.

namespace Potion
{
	// Resolves the potion form from PerkReallocation.esp and registers the consume sink.
	//
	// Safe to call repeatedly - a cheap no-op once resolution has succeeded, which is what makes
	// it usable as a rule-17 late-binding retry. Call from kDataLoaded, when the load order is
	// available. a_lastAttempt only controls whether an unresolved lookup is reported as a
	// definitive "not installed" rather than a quiet "not yet".
	void Init(bool a_lastAttempt = false);

	// True once the potion form is resolved and the sink is live. False means the plugin is not
	// installed, which is a supported configuration rather than an error.
	bool IsActive();
}
