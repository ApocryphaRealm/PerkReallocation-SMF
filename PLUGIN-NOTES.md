# Plugin notes - `PerkReallocation.esp`

The item carrier for **Perk Reallocation - SMF Settings**, this project's from-scratch C++
rebuild of *Ish's Respec Mod* (Nexus 1960). It holds the **Draught of Fate Unwound** potion, one
inert magic effect, and the two vanilla-record edits that put the potion in front of vendors -
and **nothing else**. All respec behaviour stays in `PerkReallocation.dll`; the plugin hosts no
Papyrus script.

This file records the extraction so a future session can rebuild the plugin **without redoing the
archaeology**. Everything below was read out of real files, not inferred from the readme.

---

## 1. Where the data came from

| Source | Path | What it gave |
|---|---|---|
| Original mod ESP | `C:\Modlists\Apostasy\mods\[NoDelete] 0131 Ish's Respec Mod\Ish's Respec Mod.esp` | The ALCH, MGEF, LVLI edit and CONT edit |
| Original mod BSA | `…\Ish's Respec Mod.bsa` | The one compiled Papyrus script (deliberately **not** reproduced) |
| Vanilla master | `C:\Modlists\Apostasy\Stock Game\Data\Skyrim.esm` | Authoritative data for the two overridden records + every referenced form |
| Vanilla master | `C:\Modlists\Apostasy\Stock Game\Data\Update.esm` | Checked - it touches **neither** overridden record, which is why we need only one master |

The ESP was decoded with a purpose-written TES5 record walker (group tree + subrecord walk + zlib
decompression), the BSA with a purpose-written BSA v105 reader. **xEdit's `-script:` mode is not
automatable** - it opens a modal dialog and never exits - so it was not used at any point.

---

## 2. What the original actually does

### 2a. The potion - `ALCH 0x0200182A`, EditorID `ishrespecpotion`

| Subrecord | Value |
|---|---|
| `FULL` | **`Draught of Fate Unwound`** |
| `OBND` | all zero |
| `KSIZ`/`KWDA` | 1 keyword: `0x0008CDEC` = **`VendorItemPotion`** (Skyrim.esm) - this is what makes apothecaries willing to trade it |
| `MODL` | `Clutter\Potions\PotionFortifyMagickaExtreme.nif` |
| `MODT` | `020000000000000000000000` (version 2, zero texture hashes - an empty placeholder) |
| `YNAM` | `0x0003EDBD` = `ITMPotionUpSD` (pick-up sound) |
| `ZNAM` | `0x0003EDC0` = `ITMPotionDownSD` (put-down sound) |
| `DATA` | **weight 0.1** |
| `ENIT` | **value 500**, flags `0x1` (**No Auto-Calc**), addiction NONE, addiction chance 0, consume sound `0x000B6435` = `ITMPotionUse` |
| `EFID`/`EFIT` | one effect - the MGEF below - at **magnitude 0.0, area 0, duration 0** |

The model is a **real vanilla asset** (`potionfortifymagickaextreme.nif` is present in
`Skyrim - Meshes0.bsa`), so the plugin needs no meshes or textures of its own.

Note what is **not** set: no `Medicine`, `Food` or `Poison` flag - only `No Auto-Calc`. That is the
original's own choice and is reproduced exactly rather than "fixed", because it is part of how the
item behaves in the inventory and in barter.

### 2b. The effect - `MGEF 0x020012C6`, EditorID `ishrespeceffect`

**It is script-driven, and the script is the whole mod.** `VMAD` attaches Papyrus script
**`ishrespecscript`** to the magic effect, with 20 properties: the 18 per-skill perk FormLists,
`ishBuyablePerks`, `ishStartMessage` and `ishEndMessage`.

The compiled script is the **only** file in the mod's BSA:

```
Ish's Respec Mod.bsa   (BSA v105, 1 folder / 1 file)
  scripts\ishrespecscript.pex   4147 bytes, uncompressed
     PEX header: source ishrespecscript.psc, user "Hanna", machine "HAL-11000"
```

Its string table confirms the design the SMF rebuild already reimplemented in C++:
`OnEffectStart` -> `RefundPerks`, using SKSE's `GetActorValueInfoByName` / `GetPerkTree` /
`GetSize` / `GetAt`, `HasPerk` / `RemovePerk`, and `ModPerkPoints` / `AddPerkPoints` /
`GetPerkPoints`, gated on `skse.getVersionRelease`, with the two `Message.show` calls around it.

The MGEF's own `DATA` block (152 bytes) is all defaults - **archetype 1 (Script)**, cast type 1
(Fire and Forget), delivery 0 (Self), base cost 0, no actor values (`-1`), no art, no projectile,
dual-cast scale 1.0, sound level Normal. `SNDD` is a zero-length (empty) sound array. `FULL` is
`Respec Effect`; `DNAM` is the effect description.

**Everything the effect does comes from the script. Strip the script and the record does nothing** -
which is exactly what we want from a placeholder.

### 2c. How it reaches vendors - one LVLI edit and one CONT edit

**Apothecaries** - `LVLI 0x0009CD41` `LItemApothecaryPotionMagicEffects75`, a **vanilla Skyrim.esm
record the mod overrides**. Vanilla has 7 entries (`LLCT 7`); the mod bumps it to 8 and appends
one `LVLO`: **level 1, the potion, count 1**. `LVLD` (chance-none 25%) and `LVLF` (`0x03` =
calculate-from-all-levels + calculate-for-each-item) are left at their vanilla values. This one
list is the entire "apothecaries stock it" mechanism - the readme's "leveled lists" plural is a
single list.

**The Elgrim guarantee is NOT a levelled list.** It is a second override, of
`CONT 0x000A31AE` `MerchantRiftenElgrimsElixirsChest` - the merchant container behind Elgrim's
Elixirs in Riften. Vanilla has 17 `CNTO` entries (`COCT 17`); the mod bumps it to 18 and appends
**99 copies of the potion**, flat, no levelling and no conditions. That is why Elgrim always has
some.

A detail worth knowing: Elgrim's chest *also* already contains `LVLI 0x0009CD41` (count 10) as one
of its vanilla entries, so once the levelled list is edited the potion can reach him by **both**
routes.

### 2d. Everything else in the original, and why none of it is reproduced

- **18 `FLST` perk lists + `ishBuyablePerks`** (a 632-entry list of every buyable vanilla perk) -
  pure script data. Our DLL walks `ActorValueInfo::perkTree` at runtime instead, which is also how
  it picks up modded perk trees.
- **2 `MESG` records** (`ishStartMessage`, `ishEndMessage`) - the two-stage "process has begun /
  is complete" notifications. The C++ respec is synchronous and instant, so there is no window to
  warn the player about.

---

## 3. What our plugin actually contains

```
TES4                     flags 0x00000000 (plain ESP), form version 44, master: Skyrim.esm
GRUP MGEF
  MGEF 0x01000801        PRA_RespecEffect          - NEW, inert, no script
GRUP CONT
  CONT 0x000A31AE        MerchantRiftenElgrimsElixirsChest - override, +99 potions
GRUP ALCH
  ALCH 0x01000800        PRA_DraughtOfFateUnwound  - NEW
GRUP LVLI
  LVLI 0x0009CD41        LItemApothecaryPotionMagicEffects75 - override, +1 entry
```

4 records + 4 groups = **8**, which is what `HEDR`'s count field holds (it counts records *and*
groups, excluding TES4 itself - verified against the original ESP, whose 25 records + 6 groups
match its header count of 31). `nextObjectID` = `0x802`. Top-level groups are in the engine's
canonical record-type order (MGEF before CONT before ALCH before LVLI), same as the original.

### The FormID contract

| On disk | Local ID | Type | EditorID |
|---|---|---|---|
| `0x01000800` | **`0x800`** | `ALCH` | `PRA_DraughtOfFateUnwound` |
| `0x01000801` | **`0x801`** | `MGEF` | `PRA_RespecEffect` |

The DLL resolves these as `LookupForm(0x800, "PerkReallocation.esp")`. **These must never drift** -
once the potion is in a save, its FormID is baked in.

On disk the high byte is the *master index*, and index 1 means "this file" (there is 1 master).
That is why the records read `0x01000800`, not `0x00000800`. Adding a second master would shift
them to `0x02000800` - harmless to the DLL, which resolves by plugin name, but it is why the master
list is worth keeping stable.

### Our own records, the original's values

Per the rebuild rule: **behaviour and values are reproduced; identity is ours.** Every stat that
affects play is byte-identical to the original (verified - see §5): the potion's `OBND`, `FULL`,
keyword, model, `MODT`, sounds, weight, value, `ENIT` flags, effect magnitude/area/duration, the
MGEF's whole 152-byte `DATA` block, the levelled-list entry's level and count, and the container
entry's count of 99. What is **ours**: both EditorIDs, both FormIDs, the MGEF's name
(`Fate Unwound`) and its description. No record bytes were copied from the original file - the
builder authors them from decoded values.

The potion's own name is kept as `Draught of Fate Unwound` because that *is* the thing being
reproduced.

### The inert effect

`PRA_RespecEffect` keeps archetype **Script** with **no `VMAD`**. A Script-archetype effect with no
Papyrus script attached runs nothing - the cleanest possible placeholder, and it leaves the potion
looking and behaving in the magic system exactly like the original's. It exists only so the ALCH
has an effect to point at and so the DLL has a second form to key off if it ever wants one.

### The two vanilla overrides carry real vanilla data

The engine replaces an overridden record **wholesale** - it does not merge subrecords - so both
overrides must carry the full vanilla field set. The builder re-reads them from the live
`Skyrim.esm` on every run rather than embedding blobs, so they can never drift from the master.

One deliberate deviation, and it is the same trap the `IshSoulsToPerks` job hit: **`Skyrim.esm` is
a localised plugin**, so its `CONT` `FULL` is a 4-byte string-table id (`0x0000B534`), not text.
Copying that id into our non-localised plugin would render as garbage, so the container's name is
written as the literal string `Chest`. The original mod did exactly this too.

---

## 4. Why it is a plain ESP and not light-flagged

**It is a plain `.esp`: TES4 flags `0x00000000`.** Not ESL, not ESM.

Light-flagging would work *mechanically* - a light plugin may override master records freely, and
our two new records already sit at `0x800`/`0x801`, inside the `0x800`-`0xFFF` range a light
plugin requires. So this is a deliberate choice, not a limitation.

The reason is **load order**. An ESL-flagged plugin carries the ESM flag and therefore loads in the
**master block, ahead of every regular `.esp`**. This plugin's whole job is to win two vanilla
records - an apothecary loot list and a merchant chest - and loot lists and vendor chests are
exactly the records that loot overhauls, economy mods, USSEP and merchant mods edit. Light-flagged,
this plugin would be overridden by any of them and the potion would silently vanish from vendors,
with nothing to show for it. As a plain ESP it can be sorted late (or patched), which is the only
position from which an "add my item to a vanilla list" edit can actually work.

The trade-off, stated plainly: a plain ESP costs one of the 254 regular load-order slots, whereas
a light plugin would cost none. That is the price of being able to occupy a load-order position at
all, and here the position is the point. The original mod is a plain ESP for the same reason.

**This is not hypothetical - the Apostasy load order already has the conflict.** A record-level
scan of all **3634 active plugins** in `C:\Modlists\Apostasy\profiles\Apostasy\plugins.txt` (every
one resolved to a file and parsed, zero errors) found:

| Record | Active overriders | Current winner |
|---|---|---|
| `LVLI 0x0009CD41` `LItemApothecaryPotionMagicEffects75` | **1** - `Apothecary.esp` (*Apothecary - An Alchemy Overhaul*), at position **1890 of 3634**, a plain ESP (neither ESM- nor ESL-flagged) | `Apothecary.esp` |
| `CONT 0x000A31AE` `MerchantRiftenElgrimsElixirsChest` | **0** - nothing touches it | `Skyrim.esm` |

So the Elgrim guarantee lands unopposed, but the apothecary list does not. **Light-flagging this
plugin would have put it in the master block, ahead of `Apothecary.esp`, and the potion would never
have reached a single apothecary in this modlist** - a silent failure that would have looked like a
DLL bug. As a plain ESP it can sit after position 1890 and win. That is the whole argument for §4,
made concrete.

The flip side, which needs saying on the Nexus page: our override is **wholesale**, so loading
after `Apothecary.esp` reverts *that* mod's changes to that one list back to vanilla-plus-our-entry.
For this list the practical loss is small (it is one 7-entry apothecary potion list) but it is a
real conflict, and the clean resolution is a small patch that carries Apothecary's version of the
list plus our `LVLO`, or a Bashed/Smashed patch, which merges levelled lists automatically and is
the normal answer to exactly this.

**Conflict note (general).** Because this is a wholesale record override, any other plugin that
also edits either record conflicts with it - last one loaded wins, whole record. That is inherent
to the approach and is what the original does too. If it ever becomes a real problem
in a heavy list, the alternative is to drop both overrides and have the **DLL** inject the potion
into the resolved leveled list and container at runtime (`TESLevItem`/`TESObjectCONT` entry
arrays are reachable from CommonLibSSE-NG), which conflicts with nothing - at the cost of the
plugin no longer being self-describing in xEdit. Not done here, because the brief was to reproduce
the original's distribution as the original ships it.

---

## 5. Rebuild and re-verify

```
python "D:\Claude output\.MD\scripts\Build-PerkReallocationESP.py"  <out.esp>  [Skyrim.esm]
python "D:\Claude output\.MD\scripts\Verify-PerkReallocationESP.py" <out.esp>  [original.esp] [Skyrim.esm]
```

The verifier deliberately shares no code with the builder: it re-walks the finished file from
scratch and diffs it against **both** the original ESP (the values we reproduce) and the live
`Skyrim.esm` (the vanilla data the overrides must carry), so a builder bug cannot hide itself.

### Verification performed on the shipped file

**`Verify-PerkReallocationESP.py` - all 76 checks passed.** In summary:

- ESL flag `0x200` clear, ESM flag `0x1` clear, not flagged localised
- Master list is exactly `[Skyrim.esm]`
- `HEDR` count (8) matches the actual 4 records + 4 groups; `nextObjectID` (`0x802`) is highest
  local ID + 1
- Both new records present, correct type, correct EditorID, local IDs are exactly `0x800`/`0x801`,
  written at this file's own master index; exactly two new records in the file
- Record types are exactly `ALCH, CONT, LVLI, MGEF`; every record is at SSE form version 44 and
  sits directly under its own correctly-labelled top-level group, in canonical order
- **No `VMAD` anywhere** - not one record carries a script. No `QUST`/`MESG`/`GLOB`/`FLST`/`PERK`
  records were copied
- No FormID anywhere references an index beyond the master list
- **Round-trip against the original**: `OBND`, `FULL`, `KSIZ`, `KWDA`, `MODL`, `MODT`, `YNAM`,
  `ZNAM`, `DATA`, `ENIT` and `EFIT` on the potion, and the MGEF's whole 152-byte `DATA`, are
  byte-identical; both EditorIDs and the MGEF's name and description are confirmed **different**
  from the original's
- LVLI: all 7 vanilla entries preserved in order, exactly one added (level 1, `0x01000800`, count
  1), `LLCT` agrees with the actual entry count, `LVLD`/`LVLF`/`OBND`/`EDID` preserved from vanilla
- CONT: all 17 vanilla entries preserved in order, exactly one added (`0x01000800` x99), `COCT`
  agrees with the actual entry count, all `CNTO` precede `DATA` (required field order), record
  flags (`0x08000000`) and `OBND`/`MODL`/`MODT`/`DATA` preserved from vanilla, `FULL` rewritten as
  a literal
- None of the original mod's own FormIDs appear anywhere in our plugin

**`Get-PluginRecordTypes.ps1`** reports exactly `ALCH, CONT, LVLI, MGEF`, `HasWorldRefs: False` -
nothing unexpected, and no world-space content (so this is **not** a category-6 mod).

**Every master reference resolves.** A separate pass confirmed all **26** distinct `Skyrim.esm`
FormIDs our plugin references really exist in `Skyrim.esm`, each with a sensible record type
(`KYWD`, `SNDR`, `LVLI`, `INGR`, `ALCH`, `CONT`). Zero dangling references.

### Independent validation with Mutagen (Spriggit)

Every check above uses decoders written for this job, so the plugin was additionally parsed by
**Mutagen**, a completely independent third-party implementation, via the Spriggit CLI - run with
`Throwing on unknown records: True`, so any structure Mutagen did not recognise would have been a
hard error rather than a silent skip.

```
Spriggit.CLI.exe serialize -i <plugin.esp> -o <folder> -g SkyrimSE
                           -p Spriggit.Yaml.Skyrim -v 0.41.0 -c
```

`-c` / `--Check` deserializes the YAML back into a plugin and compares, so it verifies a full
**binary round-trip** rather than merely that the file could be read. It returned **exit 0**.
`--PackageVersion` (`-v`) is mandatory. The CLI is at
`D:\Claude output\2. Mod Types\Framework or Utility\_tools\Spriggit\Spriggit.CLI.exe`.

What Mutagen independently reported:

- Mod header: **no flags at all** - it agrees the file is a plain ESP, neither ESM nor Small/light
- `MasterReferences:` a single entry, `Skyrim.esm`
- `Ingestible 000800:PerkReallocation.esp`, `PRA_DraughtOfFateUnwound`, Name
  `Draught of Fate Unwound`, keyword `08CDEC:Skyrim.esm`, model
  `Clutter\Potions\PotionFortifyMagickaExtreme.nif`, `Weight: 0.1`, `Value: 500`,
  `Flags: [NoAutoCalc]`, `PickUpSound 03EDBD`, `PutDownSound 03EDC0`,
  `ConsumeSound 0B6435`, one effect `BaseEffect: 000801:PerkReallocation.esp` with `Data: {}`
- `MagicEffect 000801:PerkReallocation.esp`, `PRA_RespecEffect`, `Archetype: Type: Script`,
  `CastType: FireAndForget`, `CastingSoundLevel: Normal`, `Sounds: []` -
  **and no `VirtualMachineAdapter` section at all**, confirming the effect carries no script
- Container resolved as **`0A31AE:Skyrim.esm`** - correctly understood as an override of a master's
  record rather than new content - `Flags: [Respawns]`, 18 items ending in
  `000800:PerkReallocation.esp` `Count: 99`
- Leveled item resolved as **`09CD41:Skyrim.esm`**, `ChanceNone: 0.25`,
  `Flags: [CalculateFromAllLevelsLessThanOrEqualPlayer, CalculateForEachItemInCount]`, 8 entries
  ending in `Level: 1 / Reference: 000800:PerkReallocation.esp / Count: 1`

### Not verified

The file was **not** opened in the SSEEdit GUI. `D:\Modlists\SME\tools\SSEEdit\` has SSEEdit
4.1.5.0 if a human wants that extra pass; it needs a person to click through the module-selection
dialog, which an agent session cannot drive - and xEdit's `-script:` mode is not an escape hatch,
since it too shows a modal dialog and never exits. The Mutagen parse above is the substitute: an
independent implementation, not the one that wrote the file.

Nothing here confirms in-game behaviour. The plugin has not been loaded by Skyrim, no apothecary
has been seen stocking the potion, and Elgrim has not been checked. That is a test-run observation,
not something static verification can supply.

---

## 6. What the DLL needs to do

The plugin is inert on its own: drinking the potion currently does **nothing**. Wiring is the
DLL's job.

**Resolve the forms once, on `kDataLoaded`** (`MessageListeners.cpp` is the natural home), and
retry/log rather than treating a miss as terminal:

```cpp
auto* dh   = RE::TESDataHandler::GetSingleton();
auto* potion = dh->LookupForm<RE::AlchemyItem>(0x800, "PerkReallocation.esp");   // ALCH
auto* effect = dh->LookupForm<RE::EffectSetting>(0x801, "PerkReallocation.esp"); // MGEF (inert)
```

A null `potion` means the ESP is not active - log it loudly and leave the SMF buttons working; the
DLL must stay functional without the plugin.

**Detect the drink.** The recommended hook is a `RE::TESEquipEvent` sink: consuming a potion fires
one with `baseObject` equal to the ALCH's FormID and `equipped == true`, with `actor` the player.
Compare `a_event->baseObject` against the resolved potion's `formID` and call the same entry point
the SMF page's button calls (`Respec.h`). `RE::TESActivateEvent` and the magic-effect-application
path are alternatives; the equip event is the simplest and does not depend on the MGEF at all,
which is why the MGEF being inert costs nothing.

**Which settings apply.** Decide deliberately: the original potion is unconditional, free and
all-skills, whereas the SMF page has scope / refund-percent / gold-cost / once-per-day settings. If
the potion routes through the same code path it inherits all of them - which is probably the right
call (the page is the mod's point) - but it is a behaviour difference from the original and should
be said out loud on the Nexus page. A per-potion bypass of the gold cost would be defensible too,
since the player already paid an apothecary 500 gold for it.

**Consume it.** The equip event fires as the potion is used, so the engine removes it; the DLL
should not remove it a second time.

---

## 7. Consequences to keep honouring

- **The plugin is a hard requirement** for the potion and ships with the mod. Without it the mod
  still works from the SMF page - the potion is an additional trigger, not a replacement.
- **The potion bakes into saves.** Once one exists in a container, a merchant's inventory or the
  player's pack, its FormID persists in any save made with the plugin active - so the plugin must
  never be casually renamed or removed, and `0x800`/`0x801` must not drift.
- **It must stay a plain ESP** unless the load-order reasoning in §4 changes.
- **The SMF page buttons stay.** Losing SMF must not lose the mechanic, and losing the ESP must
  not either.
