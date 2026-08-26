# Perk Reallocation - SMF Settings

**Version 1.0.0.** A fresh, native C++ implementation of a "respec your perks" mechanic for
Skyrim SE 1.5.97, with a real SKSE Menu Framework settings page. This is **not** a port,
fork or decompile of any existing mod - see "Where this came from" below for exactly what was
and wasn't used from the mod that inspired it.

**Licence: MIT.** Original code, written from scratch for this project - see `LICENSE`.

## What it does

- **Respec scope**: refund every skill's spent perks at once ("All Skills"), or pick one of
  the 18 skill trees to respec on its own.
- **Refund percentage** (0-100%, default 100): what fraction of the removed perks' points you
  actually get back. Every perk in scope is still removed regardless of this setting - only
  how many points you're handed back for them changes. At 100% this matches the original
  potion's all-or-nothing behaviour exactly.
- **Gold cost per point** (default 0, free): an optional gold cost charged per point actually
  refunded, checked and deducted before anything is removed - if you can't afford it, nothing
  is touched.
- **Limit to once per in-game day** (default off): refuses a second respec of any scope on the
  same in-game calendar day, tracked for the current session.
- **Respec Now** button applies the current settings immediately - no potion, no inventory
  item, nothing to craft or find. Removing perks and refunding points is instant; the settings
  above it only take effect at the moment you press it (press Save separately to keep them for
  next time, same as every other setting on this page).

## Where this came from

the author asked for a mod covering the same general idea as **Ish's Respec Mod** (Nexus
skyrimspecialedition/mods/1960) - "let the player respec their spent perk points" - but as a
brand-new native implementation, not a port. Game mechanics aren't copyrightable, only a
specific mod's own code and assets are, so this is not a derivative work of Ish's Respec Mod
and carries no licensing entanglement with it.

**What was actually read from the original**: its own Nexus page and its own included
`Ish's Respec Mod.txt` readme (both real, both cited below) - never its compiled Papyrus/BSA
content, which was never decompiled or read. The original is a single "Draught of Fate
Unwound" potion: drink it, every vanilla perk you've bought is removed and refunded (with
SKSE64 2.0.7+, perks added by other mods' trees too), no cost, no limit, no partial option -
apothecaries stock it via a levelled list, with Elgrim in Riften guaranteed to carry some.

This mod deliberately does not copy that one-shot potion design unchanged. It replaces the
potion with a settings-page button (simpler than adding a new craftable/found item, and more
in the spirit of a settings-driven SMF mod), and makes the previously-fixed behaviour
(everything, all at once, free, unlimited) into four real, independent settings instead - see
"What it does" above.

## The perk-manipulation API (the one genuinely new piece)

Enumerating and removing perks natively is a well-trodden problem, so this was researched
rather than guessed at (CLAUDE.md rule 24 - search for an existing solution before inventing
one):

- **Enumerating a skill's spent perks**: `RE::ActorValueList::GetSingleton()->GetActorValue(av)`
  gives an `RE::ActorValueInfo*`; its `perkTree` member is the root `RE::BGSSkillPerkTreeNode*`
  of that skill's perk tree. A depth-first walk of `children` (guarded against revisiting a
  node reachable through more than one parent) reaches every node; each node's `perk` field is
  the first rank of a `RE::BGSPerk*` chain linked through `nextPerk` - each rank is a separate
  point the player spent. The 18 skills are `RE::ActorValue::kOneHanded` (6) through
  `kEnchanting` (23), contiguous - confirmed directly against this project's own vendored
  `RE/A/ActorValues.h`, not assumed.
- **Removing a perk / refunding a point**: `RE::Actor::RemovePerk(BGSPerk*)` (checked with
  `Actor::HasPerk` first, so only perks the player actually has are touched), and
  `RE::PlayerCharacter::GetPlayerRuntimeData().perkCount` (a signed 8-bit field, clamped 0-127
  here) for the player's available perk-point pool.

Every one of those names was verified against this project's own vendored CommonLibSSE-NG
headers (`build/relwithdebinfo-se-only/vcpkg_installed/.../include/RE/`) - not invented blind.
The overall shape (which fields to touch, and that a DFS-with-a-visited-guard is needed
because perk trees can converge) follows a real, working, MIT-licensed reference
implementation found on GitHub: **covey-j/ActorCopyLib**
(`src/PerkUtil.cpp`, `src/ActorValueUtil.cpp`, `src/PlayerUtil.cpp`) - a CommonLibSSE plugin
that already does exactly this (its own `RefundPerks()` function). Cross-checked against the
original skse64 Papyrus binding (`ianpatt/skse64:skse64/PapyrusGame.cpp`'s
`papyrusGame::ModPerkPoints`), which confirms the same perk-point field under CommonLibSSE-NG's
current name (`perkCount`) and the original SKSE64 name (`numPerkPoints`) are the same field
Bethesda's own engine uses for `Game.ModPerkPoints` in Papyrus.

Gold cost is charged through `RE::Actor::GetGoldAmount()` / `RE::Actor::RemoveItem()` against
the vanilla Gold001 form (`0x0000000F`), both standard CommonLibSSE-NG API - no new prior art
needed there.

## Settings persistence

`Restore defaults` only resets the in-memory values to what this DLL compiles in - it never
touches the INI. `Save` and `Reload from INI` are the only two actions that touch
`PerkReallocation.ini` on disk, written with plain file I/O (never
`WritePrivateProfileString` - Mod Organizer 2's usvfs does not reliably redirect that API; see
CLAUDE.md rule 16) so a save actually reaches disk under MO2.

## Building

CMake + vcpkg, matching every other mod in this project:

```
configure.bat
build.bat
```

Requires `VCPKG_ROOT` pointing at a vcpkg checkout; Visual Studio (with the C++ toolset) plus
the CMake and Ninja it ships with are located automatically by `find-msvc.bat`. See
`CLAUDE.md` rules 4-5 for why these scripts never hardcode a machine-specific path.

## Status

First attempt - built, not yet installed or tested in game. See this project's own
`PROGRESS.md` for the current state.
