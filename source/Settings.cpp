#include "Settings.h"

#include "utils/INISettingCollection.h"
#include "utils/Logger.h"

#include <windows.h>

namespace settings
{
	using namespace utils;

	namespace
	{
		constexpr const char* kDebugSection = "Debug";
		constexpr const char* kRespecSection = "Respec";

		std::string iniPath;
		std::string iniFileName;

		// The values the plugin compiles in, captured before the INI is read so that
		// "Restore defaults" means "what you would get with no INI at all".
		struct Defaults
		{
			logger::level logLevel;

			std::uint32_t scopeIndex;
			float refundPercent;
			float goldCostPerPoint;
			bool limitOncePerDay;
		};

		Defaults defaults;

		void CaptureDefaults()
		{
			defaults.logLevel = debug::logLevel;

			defaults.scopeIndex = respec::scopeIndex;
			defaults.refundPercent = respec::refundPercent;
			defaults.goldCostPerPoint = respec::goldCostPerPoint;
			defaults.limitOncePerDay = respec::limitOncePerDay;
		}

		// One key a Save() is about to write. Queued rather than written on the spot so the
		// whole file is rewritten once at the end instead of once per key.
		struct PendingWrite
		{
			std::string section;
			std::string key;
			std::string value;
		};

		std::vector<PendingWrite> pendingWrites;

		bool EqualsIgnoreCase(std::string_view a_lhs, std::string_view a_rhs)
		{
			return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		}

		std::string_view Trim(std::string_view a_text)
		{
			constexpr std::string_view kSpace = " \t\r\n";

			const std::size_t first = a_text.find_first_not_of(kSpace);

			if (first == std::string_view::npos)
			{
				return {};
			}

			return a_text.substr(first, a_text.find_last_not_of(kSpace) - first + 1);
		}

		// Queues a key for the next FlushPendingWrites(). Cannot fail on its own - the file is
		// only touched at flush time, so that is where a write error can surface.
		bool WriteRaw(const char* a_section, const char* a_key, const std::string& a_value)
		{
			pendingWrites.emplace_back(a_section, a_key, a_value);

			return true;
		}

		// Rewrites the INI with every queued change applied in place, leaving comments and any
		// keys this plugin does not know about untouched.
		//
		// Deliberately plain file I/O rather than WritePrivateProfileString. Mod Organizer 2's
		// usvfs does not reliably redirect the Win32 profile APIs, so a call could report
		// success while never reaching disk - see CLAUDE.md rule 16. Ordinary file reads and
		// writes go through the VFS correctly.
		bool FlushPendingWrites()
		{
			if (pendingWrites.empty())
			{
				return true;
			}

			std::string text;

			{
				std::ifstream in(iniPath, std::ios::binary);

				if (in)
				{
					text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				}
				else
				{
					logger::warn("Could not read {} before saving; writing it from scratch", iniPath);
				}
			}

			const std::string newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";

			std::vector<std::string> lines;

			for (std::size_t start = 0; start <= text.size();)
			{
				const std::size_t end = text.find('\n', start);

				if (end == std::string::npos)
				{
					if (start < text.size())
					{
						lines.emplace_back(text.substr(start));
					}

					break;
				}

				std::string line = text.substr(start, end - start);

				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				lines.push_back(std::move(line));
				start = end + 1;
			}

			std::vector<bool> applied(pendingWrites.size(), false);

			std::string currentSection;

			for (std::string& line : lines)
			{
				const std::string_view trimmed = Trim(line);

				if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
				{
					currentSection = std::string{ trimmed.substr(1, trimmed.size() - 2) };

					continue;
				}

				if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#')
				{
					continue;
				}

				const std::size_t separator = line.find('=');

				if (separator == std::string::npos)
				{
					continue;
				}

				const std::string_view key = Trim(std::string_view{ line }.substr(0, separator));

				for (std::size_t i = 0; i < pendingWrites.size(); ++i)
				{
					if (applied[i] || !EqualsIgnoreCase(currentSection, pendingWrites[i].section) ||
						!EqualsIgnoreCase(key, pendingWrites[i].key))
					{
						continue;
					}

					line = std::format("{}={}", key, pendingWrites[i].value);
					applied[i] = true;

					break;
				}
			}

			for (std::size_t i = 0; i < pendingWrites.size(); ++i)
			{
				if (applied[i])
				{
					continue;
				}

				const PendingWrite& pending = pendingWrites[i];

				std::size_t insertAt = lines.size();
				bool sectionFound = false;

				for (std::size_t l = 0; l < lines.size(); ++l)
				{
					const std::string_view trimmed = Trim(lines[l]);

					if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
					{
						continue;
					}

					if (sectionFound)
					{
						insertAt = l;

						break;
					}

					if (EqualsIgnoreCase(trimmed.substr(1, trimmed.size() - 2), pending.section))
					{
						sectionFound = true;
						insertAt = lines.size();
					}
				}

				if (!sectionFound)
				{
					if (!lines.empty() && !Trim(lines.back()).empty())
					{
						lines.emplace_back();
					}

					lines.push_back(std::format("[{}]", pending.section));
					insertAt = lines.size();
				}

				while (insertAt > 0 && Trim(lines[insertAt - 1]).empty())
				{
					--insertAt;
				}

				lines.insert(lines.begin() + insertAt, std::format("{}={}", pending.key, pending.value));
				applied[i] = true;
			}

			std::string output;

			for (const std::string& line : lines)
			{
				output += line;
				output += newline;
			}

			std::ofstream out(iniPath, std::ios::binary | std::ios::trunc);

			if (!out)
			{
				logger::error("Could not open {} for writing; settings were not saved", iniPath);

				return false;
			}

			out.write(output.data(), static_cast<std::streamsize>(output.size()));
			out.close();

			if (!out)
			{
				logger::error("Could not write {}; settings were not saved", iniPath);

				return false;
			}

			logger::debug("FlushPendingWrites: wrote {} key(s) to {}", pendingWrites.size(), iniPath);

			return true;
		}

		bool WriteFloat(const char* a_section, const char* a_key, float a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{:g}", a_value));
		}

		bool WriteUInt(const char* a_section, const char* a_key, std::uint32_t a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{}", a_value));
		}

		bool WriteBool(const char* a_section, const char* a_key, bool a_value)
		{
			return WriteRaw(a_section, a_key, a_value ? "1" : "0");
		}

		// RE::INISettingCollection::GetSetting returns null for a name that is not in the
		// collection, and the templated GetSetting<T> helpers dereference that without
		// checking. Read through here instead: the value keeps whatever default it already
		// had, and the log says which setting went missing.
		template <typename T>
		T Read(INISettingCollection* a_collection, const char* a_name, T a_fallback)
		{
			if (!a_collection->GetSetting(a_name))
			{
				logger::error("Setting \"{}\" is missing from the collection; keeping the current value", a_name);

				return a_fallback;
			}

			return a_collection->GetSetting<T>(a_name);
		}

		// MakeSetting takes the setting's type from the first letter of its name - i signed,
		// u unsigned, f float, b bool, s string - and quietly hands back a setting with a null
		// name when the value passed does not match. The game's collection dereferences that
		// name, so inserting one crashes on startup with nothing useful in the log. Refuse it
		// here instead, where the message can say which setting is at fault.
		void AddChecked(INISettingCollection* a_collection, RE::Setting* a_setting, const char* a_name)
		{
			if (a_setting && a_setting->name)
			{
				a_collection->AddSettings(a_setting);

				return;
			}

			logger::critical("Setting \"{}\" was built with a value that does not match the type its "
							 "name prefix promises, so it has been skipped", a_name);
		}

		// Clamps a respec scope index read from a possibly-hand-edited INI back into range,
		// rather than trusting the file and indexing UI's kScopeNames array out of bounds.
		std::uint32_t ClampScopeIndex(std::uint32_t a_index)
		{
			constexpr std::uint32_t kMaxScopeIndex = 18;  // 0 = All Skills, 1..18 = one skill each

			return a_index > kMaxScopeIndex ? 0 : a_index;
		}

		void ReadFromCollection()
		{
			INISettingCollection* c = INISettingCollection::GetSingleton();

			{
				using namespace debug;
				const auto raw = Read<std::uint32_t>(c, "uLogLevel:Debug", static_cast<std::uint32_t>(logLevel));

				logLevel = raw <= static_cast<std::uint32_t>(logger::level::off)
							   ? static_cast<logger::level>(raw)
							   : logger::level::info;
			}

			{
				using namespace respec;
				scopeIndex = ClampScopeIndex(Read<std::uint32_t>(c, "uScopeIndex:Respec", scopeIndex));
				refundPercent = std::clamp(Read<float>(c, "fRefundPercent:Respec", refundPercent), 0.0F, 100.0F);
				goldCostPerPoint = std::clamp(Read<float>(c, "fGoldCostPerPoint:Respec", goldCostPerPoint), 0.0F, 100000.0F);
				limitOncePerDay = Read<bool>(c, "bLimitOncePerDay:Respec", limitOncePerDay);
			}
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		CaptureDefaults();

		iniFileName = a_iniFileName;
		iniPath = std::filesystem::current_path().append("Data\\SKSE\\Plugins").append(a_iniFileName).string();

		INISettingCollection* iniSettingCollection = INISettingCollection::GetSingleton();

		const auto add = [iniSettingCollection](const char* a_name, auto a_value) {
			AddChecked(iniSettingCollection, MakeSetting(a_name, a_value), a_name);
		};

		{
			using namespace debug;
			add("uLogLevel:Debug", static_cast<std::uint32_t>(logLevel));
		}

		{
			using namespace respec;
			add("uScopeIndex:Respec", scopeIndex);
			add("fRefundPercent:Respec", refundPercent);
			add("fGoldCostPerPoint:Respec", goldCostPerPoint);
			add("bLimitOncePerDay:Respec", limitOncePerDay);
		}

		if (!iniSettingCollection->ReadFromFile(a_iniFileName))
		{
			logger::warn("Could not read {}, falling back to default options", a_iniFileName);
		}

		ReadFromCollection();
	}

	bool Reload()
	{
		if (iniFileName.empty())
		{
			logger::error("Cannot reload settings before Init() has run");

			return false;
		}

		if (!INISettingCollection::GetSingleton()->ReadFromFile(iniFileName))
		{
			logger::error("Could not re-read {}; keeping the settings already loaded", iniPath);

			return false;
		}

		ReadFromCollection();

		logger::info("Reloaded settings from {}", iniPath);

		return true;
	}

	bool Save()
	{
		if (iniPath.empty())
		{
			logger::error("Cannot save settings before Init() has run");

			return false;
		}

		bool ok = true;

		pendingWrites.clear();

		ok &= WriteUInt(kDebugSection, "uLogLevel", static_cast<std::uint32_t>(debug::logLevel));
		ok &= WriteUInt(kRespecSection, "uScopeIndex", respec::scopeIndex);
		ok &= WriteFloat(kRespecSection, "fRefundPercent", respec::refundPercent);
		ok &= WriteFloat(kRespecSection, "fGoldCostPerPoint", respec::goldCostPerPoint);
		ok &= WriteBool(kRespecSection, "bLimitOncePerDay", respec::limitOncePerDay);

		ok &= FlushPendingWrites();

		pendingWrites.clear();

		if (ok)
		{
			logger::info("Saved settings to {}", iniPath);
		}
		else
		{
			logger::error("Failed to save settings to {}", iniPath);
		}

		return ok;
	}

	void RestoreDefaults()
	{
		debug::logLevel = defaults.logLevel;

		respec::scopeIndex = defaults.scopeIndex;
		respec::refundPercent = defaults.refundPercent;
		respec::goldCostPerPoint = defaults.goldCostPerPoint;
		respec::limitOncePerDay = defaults.limitOncePerDay;
	}

	const std::string& GetIniPath() { return iniPath; }
}
