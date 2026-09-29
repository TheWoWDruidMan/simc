# BracketSim engine

This is a **modified version of [SimulationCraft](https://github.com/simulationcraft/simc)**, used by BracketSim to
simulate World of Warcraft characters at Legacy brackets (levels 30 to 80). It is not the official SimulationCraft:
please do not report problems with this version to the SimulationCraft project.

Licence: GNU General Public License, version 3, the same as SimulationCraft (see `LICENSE`). Every change here is
released under that licence.

Based on upstream SimulationCraft `midnight` at commit `6ade30e` (19 September 2026,
"[Evoker] Increase default simplified player ilvl to 328"). The changes are one commit on top of it.

## What is changed, and when

- **19 September 2026: the legacy fork.**
  - Old item effects restored from the game's own spell data: procs, on-use effects and chance-on-hit weapons that
    the current engine no longer simulates. Where the modern data lost a proc rate, the rate comes from the
    [wowsims](https://github.com/wowsims) project and is cited next to it.
  - Legacy systems by level: Azerite traits and the Heart of Azeroth (40-50), essences (50), and covenant
    abilities, soulbinds, conduits and runeforge legendaries (60), in new `player/legacy_*` files.
  - Bracket rules: level caps on item effects, level-appropriate group buffs and consumables.
  - Low-level rotation fixes, so each spec's action list keeps working before every ability is learned.
- **26 September 2026: item and pet fixes.**
  - Rae'shalare, Death's Whisper and Edge of Night.
  - Xuen hang, the Arcane Echo x Harmonic Echo loop, Conjured Chillglobe's use cooldown.
  - Khaz'goroth's Courage and Norgannon's Prowess proc callbacks.
  - Masquerade Gown (Love Struck).
  - Balance Druid scaling: Intellect / spell power are weighed, not Agility.
- **27 September 2026: level-60 covenant abilities, soulbinds and a second Chillglobe fix.**
  - Covenant abilities: Wild Spirits, Death Chakram, Resonating Arrow, Decimating Bolt, Soul Rot / Satchel,
    Elysian Decree, Abomination Limb, Celestial Spirits.
  - Soulbinds and conduits: Grove Invigoration stacks, Exploiter, Seeds of Rampant Growth.
  - Chillglobe's use proxy can only be fired by its item (an off-GCD trinket line looped it).
- **29 September 2026: healer damage and Acid Rain.**
  - Restoration Shaman: Acid Rain (talent 378443) - Healing Rain deals Nature damage to enemies inside it while the
    talent is taken; Healing Rain resolves from the talent when the spec spell no longer does.
  - Holy Paladin: Judgment uses Holy's own spell (275773); Holy Shock resolves from its spell data; attack power from
    spell power (104%, passive 1258016).
  - Mistweaver: attack power from spell power (passive 1258138) and Intellect as the converted primary stat.
  - Comment-only updates in other files.

## Building

Build exactly as upstream SimulationCraft (CMake). See `README.md`.
