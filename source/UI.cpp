#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Respec.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		// The slider the arrow keys currently drive. Set by clicking one.
		std::string selectedSlider;

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		// The framework renders from the renderer's present hook, which is not the thread the
		// game's own systems expect to be talked to from - anything beyond touching this
		// plugin's own settings variables has to be handed to the main thread first. This
		// matters more for this mod than most: PerformRespec() touches live actor and
		// inventory state.
		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface())
			{
				taskInterface->AddTask(std::move(a_task));
			}
		}

		// See AutoDraw-SMF/source/UI.cpp's identical check (itself ported from
		// CompassNavigationOverhaul/source/UI.cpp - CLAUDE.md rule 24) for the reasoning: older
		// SMF builds do not export every cimgui function a page needs, and calling through a
		// null function pointer crashes on the first draw rather than failing to register - so
		// every export this page's widgets resolve at runtime is probed here first, by its
		// *resolved* name (varargs widgets resolve to a "...V"-suffixed export).
		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabledV",
				"igTextWrappedV",
				"igSetTooltipV",
				"igSeparatorText",
				"igCombo_Str_arr",
				"igSliderFloat",
				"igIsItemHovered",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igPushItemWidth",
				"igPopItemWidth",
				// Needed by NudgeableSlider's arrow-key nudge.
				"igIsKeyPressed_Bool",
				"igIsItemClicked",
				"igIsItemActive",
				// Needed by utils/Toggle.h's hand-drawn switch (CLAUDE.md rule 32 - boolean
				// settings render as a switch, not a checkbox).
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
			};

			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("SKSE Menu Framework does not export \"{}\"", name);

					return false;
				}
			}

			return true;
		}

		// A slider that the arrow keys can also nudge, once it has been clicked. Ported
		// verbatim from Dragon's Eye Minimap's UI.cpp via AutoDraw-SMF - CLAUDE.md rule 24.
		bool NudgeableSlider(const char* a_label, float* a_value, float a_min, float a_max,
							 const char* a_format, float a_step)
		{
			bool changed = ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);

			if (ImGuiMCP::IsItemClicked() || ImGuiMCP::IsItemActive())
			{
				selectedSlider = a_label;
			}

			if (selectedSlider == a_label)
			{
				float nudge = 0.0F;

				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow))
				{
					nudge -= a_step;
				}
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_UpArrow))
				{
					nudge += a_step;
				}

				if (nudge != 0.0F)
				{
					*a_value = std::clamp(*a_value + nudge, a_min, a_max);
					changed = true;
				}

				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("<-->");
			}

			return changed;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("(?)");

			if (ImGuiMCP::IsItemHovered())
			{
				ImGuiMCP::SetTooltip("%s", a_description);
			}
		}

		void RenderRespecSection()
		{
			using namespace settings::respec;

			ImGuiMCP::SeparatorText("Respec");

			int scope = static_cast<int>(scopeIndex);
			if (ImGuiMCP::Combo("Scope", &scope, Respec::kScopeNames, Respec::kScopeCount))
			{
				scopeIndex = static_cast<std::uint32_t>(scope);
			}
			HelpMarker("Which skill's spent perks to refund - or every skill at once.");

			NudgeableSlider("Refund percentage", &refundPercent, 0.0F, 100.0F, "%.0f%%", 5.0F);
			HelpMarker("What fraction of the removed perks' points you actually get back. Every perk in "
					   "scope is still removed either way - this only changes how many points you're "
					   "handed back for them, matching Ish's original potion at 100%%.");

			NudgeableSlider("Gold cost per point", &goldCostPerPoint, 0.0F, 1000.0F, "%.0f", 10.0F);
			HelpMarker("Gold charged per perk point actually refunded. Defaults to 500, so a "
					   "ten-perk respec costs 5,000 gold. Set it to 0 to make respecs completely "
					   "free, matching the original potion.");

			ImGuiMCP::Toggle("Limit to once per in-game day", &limitOncePerDay);
			HelpMarker("Refuses a second respec (of any scope) on the same in-game calendar day. Off by "
					   "default, matching the original, which had no limit at all.");

			ImGuiMCP::Spacing();

			if (ImGuiMCP::Button("Respec Now"))
			{
				const std::uint32_t requestedScope = scopeIndex;

				OnMainThread([requestedScope]() {
					const Respec::Result result = Respec::PerformRespec(requestedScope);

					statusMessage = result.message;
				});
			}
			HelpMarker("Removes every perk you own in the scope above right now and refunds points "
					   "according to the settings above.");
		}

		void RenderDebugSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Debug");

			int level = static_cast<int>(debug::logLevel);
			if (ImGuiMCP::Combo("Log level", &level, kLogLevelNames, kLogLevelCount))
			{
				debug::logLevel = static_cast<logger::level>(level);

				OnMainThread([]() { logger::set_level(settings::debug::logLevel, settings::debug::logLevel); });
			}
			HelpMarker("Applies to the log immediately. Ships at Trace by default - see CLAUDE.md rule 31.");
		}

		void RenderButtons()
		{
			if (ImGuiMCP::Button("Save"))
			{
				OnMainThread([]() {
					statusMessage = settings::Save() ? "Settings saved." : "Could not save the INI. See the log for why.";
				});
			}
			HelpMarker("Writes every setting above back to the INI. Comments and unrelated keys are left alone.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Reload from INI"))
			{
				OnMainThread([]() {
					statusMessage = settings::Reload()
										 ? "Settings reloaded from the INI."
										 : "Could not read the INI. See the log for why.";
				});
			}
			HelpMarker("Throws away any change made here since the last save and re-reads the INI from disk. Also picks up edits made to the file by hand.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				OnMainThread([]() { settings::RestoreDefaults(); });

				statusMessage = "Defaults restored. Press Save to keep them.";
			}
			HelpMarker("Puts every setting back to the value it has on a fresh install. Nothing is written until you press Save.");

			if (!statusMessage.empty())
			{
				ImGuiMCP::TextWrapped("%s", statusMessage.c_str());
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("SKSE Menu Framework is not installed; settings will be read from the INI only");

			return;
		}

		if (!HasRequiredExports())
		{
			logger::warn("The installed SKSE Menu Framework is older than this plugin's settings "
						 "menu needs. Update it to a newer version to configure Perk Reallocation in game.");

			return;
		}

		SKSEMenuFramework::SetSection("Perk Reallocation");
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);

		logger::info("Registered the settings page with SKSE Menu Framework");
	}

	void __stdcall SettingsPanel::Render()
	{
		ImGuiMCP::TextWrapped("Removing perks and refunding points happens immediately when you press "
							  "\"Respec Now\" - the settings above it only apply as of the moment you press it. "
							  "Press Save separately to keep them for next time.");
		ImGuiMCP::Spacing();

		ImGuiMCP::PushItemWidth(260.0F);

		RenderRespecSection();
		ImGuiMCP::Spacing();

		RenderDebugSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
