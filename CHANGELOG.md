# PerkReallocation-SMF - changelog

Rule 61: this mod's own history, kept beside the code it describes.

> **The entries below this line were RECONSTRUCTED from `version-ledger.json` on
> 2026-08-27, not written at the time of the change.** They carry only what the ledger
> recorded - the status and the evidence - so they are thinner than a real entry and may
> be missing changes the ledger never captured. Treat them as a starting point rather
> than a record. Everything from the next version onward is written as it happens.

Each version carries its **version-ledger status**: **working** (observed in game),
**untested** (built, not confirmed), **failed** (built but broken; the number was
reclaimed), **scratch** (a hypothesis-test build that never held a real number).

## 1.0.2 - 2026-08-27 - untested

### Changed
- local package built from an uncommitted working tree; 1.0.1 exists nowhere at all - see VERSION-RECONCILIATION.md ambiguity 6

### Known
- LATENT BUG, verified by source inspection 2026-08-27, not yet fixed here: this mod saves its INI with plain file I/O but reads it back through INISettingCollection::ReadFromFile, which uses the Win32 profile API that PrivateProfileRedirector hooks and caches. Under the Redirector a reload is served the values from game start rather than the ones just written - settings appear to save then revert. Worse, once the plugin's INI has been read through that API the Redirector caches it and can write its stale copy back over the file on game-save or exit, losing settings between sessions. Dragon's Eye Minimap 1.5.7 fixed exactly this: prefer values parsed directly from the file in the shared Read<T> helper, and stop calling ReadFromFile so the Redirector never caches our INI at all. CustomDifficultyUI-SMF was fixed earlier and is the precedent.

## 1.0.0 - 2026-08-27 - working

### Changed
- git tag v1.0.0 pushed

