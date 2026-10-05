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
- **30 September 2026 (night): azerite traits across specs.** An audit of every trait against every other spec of its
  class: Arcanic Pulsar now adds its flat Starsurge damage (never modelled, and missing entirely on the off-spec
  Starsurge), Replicating Shadows works for any spec with Rupture, Inner Light's damage exists for every paladin spec,
  and Lava Shock empowers Earth Shock as its spell data says (it was on Lava Burst).
- **1 October 2026: SimulationCraft merged up to date.** 73 upstream changes, including live game data build 69933
  (Blizzard's hotfixes, e.g. Shadow Priest and Augmentation tuning) and upstream rotation updates. Mage's Frostfire /
  Ice Lance refactor taken as upstream wrote it, with the legacy hooks (Duplicative Incineration, Molten Skyfall,
  Flames of Alacrity, Slick Ice, Tunnel of Ice, Cold Front, Glacial Fragments, Packed Ice, Whiteout) re-applied.

## Building

Build exactly as upstream SimulationCraft (CMake). See `README.md`.

### The in-browser engine ("Run on my PC")

The WebAssembly engine bracketsim.gg sends to browsers is built from this branch with Emscripten (emsdk) and CMake:

```
emcmake cmake -G Ninja -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release -DSC_NO_NETWORKING=ON -DSC_NO_THREADING=ON \
  -DBUILD_GUI=OFF "-DCMAKE_CXX_FLAGS=-O3 -DNDEBUG -fwasm-exceptions -msimd128" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG" \
  "-DCMAKE_EXE_LINKER_FLAGS=-O3 -sMODULARIZE=1 -sEXPORT_NAME=createBracketSimC -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 \
   -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,worker,node -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS \
   -sASSERTIONS=0 -fwasm-exceptions -sMALLOC=mimalloc"
cmake --build build-wasm --target simc
```

`simc.js` and `simc.wasm` are served as `engine/bracketsim-engine.js` and `engine/bracketsim-engine.wasm`.

Since 1 October 2026 the WebAssembly build defines NDEBUG like the native release build (the release flags had dropped
it, leaving every assert() active in the browser): 1.8x faster, identical output. `build-wasm-fast.bat` is the Windows
script that runs these steps.

- 2026-10-02: crafted consumables (Dragonflight / The War Within flasks, phials, potions) honour `bracketsim_consumable_ilvl` too. Flask of Tempered Aggression reads 21 Critical Strike on a level 80 in game; the engine gave 56. With the tooltip item level (23) it gives 21. (`engine/player/consumable.cpp`)
- 2026-10-03: a taken talent's spell is the player's at any level (the talent tree decides when it is learnt, not the spell's own level). Talents whose spell is listed above the character's level were taken but did nothing: Power Infusion (level 58) on every level 30-57 priest, Combat Potency for Outlaw, Shadow Blades for Subtlety, and about 40 more. The Power Infusion buff is always built from the full spell, so a Power Infusion from another priest (`external_buffs`) also works below 58. (`engine/player/talent.cpp`, `engine/class_modules/priest/sc_priest.cpp`)
- 2026-10-03: Pendant of the Violet Eye and Meteorite Crystal give one Versatility stack per spell or ability CAST, as combat logs show in game (five players, five classes): not per hit, not from item uses, and not from Shadow's Void Volley, whose cast hits nothing itself. A Devourer's Void Ray still gives one per beam tick, as in game. The old per-hit rule gave a level-30 Frost mage 45 stack gains a use from Ray of Frost ticks and a Shadow Priest about 7. New `player_t::callbacks_on_cast` sees every foreground action, including abilities that switch procs off (Mutilate). Shadow's opener no longer casts a second Tentacle Slam while the pre-pull one is in flight (the engine let the pre-pull Slam skip its global cooldown): Slam pre-pull, Shadow Word: Pain, then the normal opener. (`engine/player/unique_gear.cpp`, `engine/player/player.hpp`, `engine/action/action.cpp`, `engine/class_modules/apl/apl_priest.cpp`, `ActionPriorityLists/default/priest_shadow.simc`)
- 2026-10-03: on-use trinkets the rotation never presses are pressed on cooldown from the pull (`bracketsim_trinket_fallback`, on by default, `=0` turns it off). Each actor's first fight is the probe: a trinket the rotation presses at any point, pre-pull included, is left to the rotation. Instant uses only (a cast or channel can cost casting time). Many rotations hold buff trinkets for a cooldown a low-level character has not learned (Arms waits for Avatar, Fury for Recklessness, Windwalker holds Fists of Fury for its trinket) and the healer damage lists have no trinket line: 78 of the 320 test characters, every bracket 30-80, never pressed one. Pendant of the Violet Eye on those: +1% to +26% (Windwalker), average +5.6%; the 320 characters' own gear unchanged. JSON report: pets that share a name (8 Nibelung Val'kyr) are written as "<name>", "<name>#2"... instead of overwriting one key. (`engine/player/player.cpp`, `engine/player/player.hpp`, `engine/report/json/report_json.cpp`)
- 2026-10-03: new expression `bracketsim_cast_stack_window_remains` - seconds left on a Pendant of the Violet Eye / Meteorite Crystal window, 0 when neither is worn - so a rotation can save a big spell for the end of the window without naming a buff that only exists while the trinket is worn. Shadow holds Void Volley for the last 5 seconds of the window: +3.6% on a level 30 Shadow with the Pendant (5 s beat 3, 7 and 9); characters without these trinkets are unchanged. (`engine/player/player.cpp`, `engine/class_modules/apl/apl_priest.cpp`, `ActionPriorityLists/default/priest_shadow.simc`)
- 2026-10-04: a Devourer's Void Ray stacks Pendant of the Violet Eye / Meteorite Crystal only with its ticks OUTSIDE Metamorphosis (`void_ray_tick`), as players report in game; Meta's ticks (`void_ray_tick_meta`) no longer do. Level 30-34 Devourer rotation (a player's in-game plan, measured): press the Pendant / Crystal and one Void Ray as Meta comes up, no Void Ray again until Meta is running out, Soul Immolation only as end-of-Meta filler: +5.4% at 30 with the trinket, +1.0% without. It loses 3-13% at 35-70, so level 35 and up keep the list as it was. (`engine/player/unique_gear.cpp`, `engine/class_modules/apl/apl_demon_hunter.cpp`, `ActionPriorityLists/default/demonhunter_devourer.simc`)
- 2026-10-04: "+N Attack Power / Spell Power versus <creature type>" on gear and enchants (Undead Slayer, Undead/Demon Slayer, Felbane weapons...) is read: when the target's race (`target_race`) is in the bonus's creature-type mask it is added as a gear stat, scaled to the item. These auras were never read, so they simmed as nothing against any target. (`engine/player/player.cpp`)
- 2026-10-04: The Twin Blades of Azzinoth (2) "+30 attack power when fighting Demons" (in-game tooltip) is applied when `target_race=demon`; it was left out while the target could not be anything but humanoid. (`engine/player/player.cpp`)
- 2026-10-04: Grove Invigoration (Night Fae soulbind) grants Redirected Anima per class, "(8 * N) stacks" with N from the game data: Death Knight 2, Monk 4, Mage and Paladin 6, Warlock 8, Priest, Rogue, Warrior and Demon Hunter 12, Druid, Hunter and Shaman 16. Every class got 8; a level 60 Blood DK averaged 17 stacks where the game gives 2 per cast (now 5). (`engine/player/legacy_soulbind_effects.cpp`)
- 2026-10-04: Pharamere's Forbidden Grimoire's Orb of Destruction fires for ranged and caster specs. It refused under a 14 yard gap, and every actor stands at distance 0-10 unless distance targeting is on, so it never fired for anyone; its tooltip asks for 10 yards and a ranged character stands well outside that in game. (`engine/player/unique_gear_legion.cpp`)
- 2026-10-04: `target_race` now reaches the enemy. enemy_t::init_base_stats forced RACE_HUMANOID after init_race had applied target_race, so every creature-type effect only ever saw a humanoid: an Adventurer's Journal roll for any other type (bracketsim_creature_damage) added nothing, and Demonsbane-style conditions could never pass. Humanoid is now forced only when target_race is empty. (`engine/class_modules/sc_enemy.cpp`)
- 2026-10-04: Houndmaster's Weapons (Magma-Shot Boomstick, Houndmaster's Bow, driver 470629) - "additional Physical damage to Beasts" - fires against beasts only; it was a plain trigger and fired on every target. (`engine/player/unique_gear.cpp`)
- 2026-10-04: a proc on damage TAKEN whose action harms (Brand of Ceaseless Ire, All-Devouring Nucleus, Naglering-style "when struck") is aimed at the attacker. The incoming path passed the victim - the listener itself - as the target, so the "do not harm your own side" check discarded every such proc: on a tank (role=tank) hit by the boss they never fired. Level 80 Protection paladin with both trinkets: Brand 1,789 DPS, Devouring Void 811. A damage dealer is still never hit on Patchwerk, so nothing changes there. (`engine/action/dbc_proc_callback.cpp`)
- 2026-10-04: fourteen Shadowlands conduits ported from SimulationCraft's Shadowlands code, each proven with the same seed with vs without at level 60 (rank 11): Hunter - Enfeebled Mark (+3.4% BM Kyrian), Spirit Attunement, Necrotic Barrage, Sharpshooter's Focus, Powerful Precision, Stinging Strike (+3.2% Survival); Mage - Infernal Cascade, Siphoned Malice (+4.1% Arcane Venthyr), Flame Accretion; Priest - Swift Penitence; Monk - Imbued Reflections, Strike with Clarity; Death Knight - Brutal Grasp; Druid - Conflux of Elements (on only while a Convoke channel runs; the player multiplier is cached, so Convoke's execute and last tick invalidate it). (`sc_hunter.cpp`, `sc_mage.cpp`, `priest/sc_priest_discipline.cpp`, `monk/sc_monk.cpp`, `sc_death_knight.cpp`, `sc_druid.cpp`)
- 2026-10-04: Weapons of Order (Kyrian monk) adds its Mastery as points, as the Shadowlands code did. The value was scaled by 0.01, so the buff gave 0.1 Mastery points - about nothing. Level 60 Windwalker Kyrian +3.6%, Brewmaster +2.4%. (`engine/class_modules/monk/sc_monk.cpp`)
- 2026-10-04: Strength of Earth (Enhancement azerite) ported from SimulationCraft's BfA shaman code: Flame Shock grants it (the game text now names Flame Shock; BfA used Rockbiter, which is gone), and the next melee ability that is not an auto attack deals the trait's Nature damage. Level 50 Enhancement, item level 60 trait: +1.66% on 1 target, +0.56% on 3. (`engine/class_modules/sc_shaman.cpp`)
- 2026-10-04: Focused Light (Holy Paladin conduit) adds its value to Holy Shock's critical chance, damage and heal. SimulationCraft's Shadowlands code declared it and never applied it. Level 60 Holy Paladin damage list, rank 11: +1.13%. (`engine/class_modules/paladin/sc_paladin_holy.cpp`)
- 2026-10-04: level 30 Devourer with Pendant of the Violet Eye or Meteorite Crystal holds Metamorphosis for the trinket's window unless waiting would cost a Meta (the trinket is back too late for this fight, or there is time for another Meta after the held one). Same seed, 6,000-8,000 iterations: 3 min +1.6%, 4 min +0.9%, 5-7 min +0.3%, never below the old line; holding every time lost 2-3% at 5-7 min (one Meta fewer). Levels 35+ unchanged. (`engine/class_modules/apl/apl_demon_hunter.cpp`, `ActionPriorityLists/default/demonhunter_devourer.simc`)
- 2026-10-05: twenty Shadowlands runeforge legendaries and six conduits made to act, each proven with the same seed with vs without at level 60 on a build that uses the ability. New code: Akaari's Soul Fragment (+4.2% Subtlety), Deeply Rooted Elements (+20.9% Elemental, +9.5% Enhancement), Deathmaker (Siegebreaker's +15% damage taken; +6.5% Fury), Odr, Shawl of the Ymirjar, Implosive Potential, Shadowflame Prism, Divine Image (25% per Holy Word to summon the Naaru the Midnight talent models), Charred Passions and Jade Ignition (the legendary switches the talent's code on; the stronger value wins when both are taken), Shaohao's Might, Last Emperor's Capacitor, Lycara's Fleeting Glimpse (Moonkin Form only), Cat-Eye Curio, Bulwark of Righteous Fury, Reanimated Shambler, and Perforated Veins, Well-Placed Steel and Strength of the Pack. Mapped onto the Midnight successor of the ability they named: Malefic Wrath and Focused Malignancy onto Malefic Grasp (Malefic Rapture), Dissonant Echoes onto Void Volley (Void Bolt), Deadly Tandem onto Takedown (Coordinated Assault). Elemental Equilibrium's 15% was never applied (the buff rose and nothing read it). Enhancement's single-target lists gained a plain Windstrike line, because Ascendance blocks Stormstrike and nothing below level 71 pressed Windstrike. All 320 test characters unchanged without these items. (`sc_rogue.cpp`, `sc_shaman.cpp`, `sc_warrior.cpp`, `warlock/*`, `priest/*`, `monk/sc_monk.*`, `sc_druid.cpp`, `paladin/*`, `sc_death_knight.cpp`, `sc_hunter.cpp`, `ActionPriorityLists/default/shaman_enhancement.simc`)
- 2026-10-05 (later): the last four Shadowlands legendaries left over act. Toxic Onslaught (Night Fae): when Sepsis expires you gain the other specs' cooldowns for 10 sec - Adrenaline Rush, Shadow Blades and Deathmark (Vendetta's successor) fall back to their base spells for a spec without the talent; level 60 Assassination +6.7%, Outlaw +2.2%, Subtlety +1.5%. Witch Doctor's Wolf Bones brings back the Feral Spirit button it shortened (90 sec, two Spirit Wolves, a Maelstrom Weapon stack every 3 sec). Serpentstalker's Trickery: Aimed Shot fires a Serpent Sting built from the Shadowlands spell (271788), which Midnight no longer has (+0.9% Marksmanship). Lycara's Fleeting Glimpse now covers Cat Form: a second Primal Wrath found the shared "rip_primal" action and deleted the first one's stats object, crashing at start-up - each Primal Wrath now owns its Rip. Chain Heal is a class talent in Midnight but was looked up as a class spell, so it never cast (Chains of Devastation +5.2% at 3 targets when Chain Heal is cast). 320 test characters unchanged. (`sc_rogue.cpp`, `sc_shaman.cpp`, `sc_hunter.cpp`, `sc_druid.cpp`)
