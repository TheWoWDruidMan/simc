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
- **29 September 2026 (later): spell levels and Acid Rain's row.**
  - A spell a taken talent teaches is usable at any level (the talent tree decides when it is learnt, as in game).
  - Fire Blast (108853) is learnt below level 30: spell level set to 13 through the hotfix system, as upstream does
    for Felblade. Level-30 mages were simmed without it.
  - Acid Rain damage has its own row instead of being counted under Healing Rain.
- **29 September 2026 (night): three trinkets and a Devourer rule.**
  - Pendant of the Violet Eye and Meteorite Crystal: every spell or special attack in the 20 seconds after use adds a
    stack (up to 20), and all stacks go when it ends. The engine had added one stack per use and never removed it.
  - Dragonspine Trophy: 1 proc per minute behind its 20 second cooldown (as wowsims), not 100% on the first hit after
    the cooldown.
  - Devourer Demon Hunter: effects that only proc from class abilities proc only from the Void Ray tick outside
    Metamorphosis, as players report from the live game.
- **30 September 2026: on-use weapons.** A weapon with a use effect (Fyr'alath the Dreamrender) that no action list
  mentions is pressed like the other legacy buttons; most specs only press on-use items by trinket slot, so its
  Rage of Fyr'alath was never used.
- **30 September 2026 (later): five azerite traits' damage.** Holy Paladin: Glimmer of Light and Radiant
  Incandescence. Discipline Priest: Contemptuous Homily. Vengeance Demon Hunter: Essence Sever and Cycle of Binding.
- **30 September 2026 (night): Death's Due replaces Death and Decay.** Night Fae Death Knights cast Death's Due in
  place of Death and Decay (one shared cooldown, one ground effect, every Death and Decay bonus still applies) instead
  of both. Heart Strike, Scourge Strike, Clawing Shadows and Obliterate inside it steal Strength (324165).
- **30 September 2026 (late): old flasks and elixirs at the item's level.** A consumable whose spell "Scales with
  Casting Item's Level" (354) is valued at the item level the game gives it, passed as
  `bracketsim_consumable_ilvl=<item id>:<item level>/...`; read at the character's level it came out about double
  (Elixir of the Mongoose 10 Agility and 3 Critical Strike, 5 and 1 in game). Unset, nothing changes.
- **30 September 2026 (late): Light's Decree for every paladin spec.** In game it can be picked as Retribution and
  keeps working as Holy or Protection (every Holy Power spent during Avenging Wrath); its damage action was only
  created for Retribution.

## Building

Build exactly as upstream SimulationCraft (CMake). See `README.md`.

### The in-browser engine ("Run on my PC")

The WebAssembly engine bracketsim.gg sends to browsers is built from this branch with Emscripten (emsdk) and CMake:

```
emcmake cmake -G Ninja -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release -DSC_NO_NETWORKING=ON -DSC_NO_THREADING=ON \
  -DBUILD_GUI=OFF "-DCMAKE_CXX_FLAGS=-O3 -fwasm-exceptions -msimd128" "-DCMAKE_CXX_FLAGS_RELEASE=-O3" \
  "-DCMAKE_EXE_LINKER_FLAGS=-O3 -sMODULARIZE=1 -sEXPORT_NAME=createBracketSimC -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 \
   -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker,node -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS \
   -sASSERTIONS=0 -fwasm-exceptions -sMALLOC=mimalloc"
cmake --build build-wasm --target simc
```

`simc.js` and `simc.wasm` are served as `engine/bracketsim-engine.js` and `engine/bracketsim-engine.wasm`.
