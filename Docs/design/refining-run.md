# Airlock — the refining run: one MVP mission with a reason to exist (2026-09-12)

A design note that takes Tom's rough pitch — *a hauler picks up a crude-oil-like substance at a moon extractor, carries it to a solar refinery, the stuff poisons the crew and spills until the scrubbers are on and the barrels are racked, the ship runs dry mid-voyage and limps in by burning the crude itself, the refinery refines it, refuels the ship and sends the surplus fuel home* — and does three things with it: gives every hazard a real physical basis so the mechanics fall out of the material rather than being bolted on; maps the run onto what is already shipped (M2 through M3-4); and proposes it as **the** MVP mission, with the minute-by-minute run and a re-plan of M3-5 to M3-7.

Companion docs: `airlock-voyage-loop-design-2026-09.md` (the verbs, the ship graph, the pacing curve, the five coordination levers) and `Airlock/Docs/STATUS.md` (what is verified on `main`).

## 1. The verdict first

Adopt it. Four reasons:

1. **It gives the hauling run a why.** Today the board offers "mining, pirate interdiction, derelict salvage" — labels on the same run. The refining run is a closed economic loop: the extractor is fuel-poor, the refinery is crude-poor, the hauler is the artery between them. Every run ends with a number the base needs (fuel), which is the hub payoff M3-7 has been waiting for.
2. **Nothing shipped is wasted.** Casks are the volatile crates we have; the cold rack is the holding bay; the cargo shift, the carry verb, the hold-E repair, the control seat, the task board and the tally all keep their jobs. The lore renames them; the code does not.
3. **It adds exactly two systems and one place**: a ship atmosphere with scrubbers, a fuel system with the crude bypass, and the refinery as the destination. Each is one Astra-sized brief.
4. **The fuel crisis is the interdependence the first design note asked for.** Burning the cargo to reach the dock is "cannibalize" with real stakes — you feed the engines the thing you are paid to deliver — and choosing *which* cask to burn (the one already warming?) is a decision, not a chore.

What it replaces: the three voyage types become three **contract tiers** of the same run (4, 8 and 12 casks); M3-5 "cannibalize + calibrate" is folded into the fuel beat (burning casks is the cannibalize, holding the bypass mixture is the calibrate).

## 2. The world, and why the stuff behaves the way it does

Names are placeholders (Tom picks or vetoes); section ids in code (`StationHub`, `Hauler_Mk1`) never change.

**Vell** is an ice moon with a thick nitrogen atmosphere and seas of liquid methane and ethane, orbiting the gas giant **Halder** — Titan with a licence. Ultraviolet and charged-particle chemistry in its haze makes **tholins**: heavy, nitrogen-rich organics that rain into the seas. The pumps at the **Vell Extractor** (the Rig — our hub's dock deck) draw a cryogenic slurry of sea liquid and suspended tholins. Crews call it **sludge**; the manifest calls it **Vell crude**.

Every hazard in the run is a real property of that slurry:

| Property of Vell crude | What it does in the game |
|---|---|
| Cryogenic, about 92 K. A cask that warms boils off, pressure rises, the relief valve puffs, and a cask left warm long enough ruptures. | The existing heating crates: loose or carried casks warm after the calm minutes, the rack keeps them cold, 100 C is now "ruptured". The relief puff is the spill Tom asked for. |
| The boil-off carries **nitriles** — hydrogen cyanide, acetonitrile, cyanoacetylene — the same compounds Cassini measured over Titan. Tens of ppm of HCN gives headache and dizziness over minutes; hundreds kill. | Gradual crew poisoning: a per-section contaminant level that venting casks raise, and a per-crew exposure meter with effects. Scrubbers are the answer because they are the answer on real stations (trace-contaminant control: catalytic oxidiser and adsorbent beds). |
| Methane and ethane vapour displace oxygen and burn in air at 5-15 %. | Post-MVP: a ruptured cask in a pressurised hold is a fire incident and the extinguisher verb. |
| Methane **is** rocket fuel: methalox engines burn liquid methane and oxygen. Raw slurry can be fed through a crude filter and bypass — lower specific impulse, tholin solids fouling the injectors, soot. | The mid-run reversal: open the bypass, feed the engines casks, hold the mixture, clear the injectors, pay a soot fine at the dock. |
| Crude is worthless until cracked; cracking is energy-hungry. Refined, it is fuel (methane), polymer feedstock (ethane, propylene), chemistry feedstock (nitriles) and nitrogen. | Why the refinery exists, why it pays, and why it pays partly **in fuel**. |

**Halder Solar Refinery** sits in a high, sunlit orbit on the moon's sunward side: a kilometre-wide field of concentrator mirrors feeding solar-thermal crackers and electric separators, because reactors near crewed stations are licensed and expensive while sunlight, thin as it is out here, is free at scale. The mirror field is the brightest thing in Vell's sky — the destination you can see from the Rig's windows (a sky element for the map pass).

**Hauler Mk1** is a methalox hauler with a cryo hold (transit clamps and one cold rack), a crude bypass the yard fitted and the manufacturer never approved, an NPC captain who flies, and one to four crew who keep it alive.

**The loop that makes it a campaign:** the Rig burns fuel every day and cannot make any. Each run's surplus refined methane comes home in the hauler's tanks and raises the **base fuel stock**, a number on the departure board that only goes up when crews deliver. Later that number can gate offers (a base running dry posts priority runs with bigger pay and worse hazards); in the MVP it just goes up, visibly.

Honesty box: the twenty-minute flight is the same abstraction the current voyages use; temperatures, chemistry and fuel are the real ones; the two bends are that a bypass can burn slurry at all (a real engine would need a redesign) and that a section's atmosphere is one uniform number.

## 3. The run, minute by minute (20 min, Hauler Mk1, 1-4 crew)

The pacing skeleton is the one from the first note — calm, rising, climax, dock — with the fuel beat as a fixed reversal in the middle and the director's random draws around it.

- **0-3 Undock and secure.** Captain: "Underway. Sludge casks are loose on the deck — clamp them, or rack them, the rack's cold." Solo manifest four casks, crewed up to twelve. Teaches Carry, clamps and the rack with no pressure. *Shipped.*
- **3-6 First puff.** A loose cask (or one a failed clamp let go) warms; its relief valve hisses a green-white haze; the task board's air line starts climbing: `Hold air 14 ppm - scrub`. Captain: "Scrubbers to hazard mode — bridge board or the life-support panel." Switching them is a press at the Life Support module or a toggle in the control seat. Hazard mode draws power; the board shows the load, and the cold rack's cooling slows while power is short. Then rack the cask. Teaches the atmosphere, the scrubbers and the exposure meter. *New (M3-5).*
- **6-9 Lines.** The ramshackle ship does what it does today: a coolant leak, a breaker trip, hold-E repairs from the walkway. One new dependency: the cold rack runs off the coolant loop, so a Degraded loop halves the rack's cooling — a leak in engineering is now a cargo problem too. *Shipped plus one line in the model.*
- **9-12 The fuel beat.** A fuel feed valve fails open. The gauge on the board drops from 61 % towards nothing; the captain: "We're bleeding methane. Engineering — the red valve, shut it." Hold-E closes the valve (a new `Fuel` line kind on the existing hold verb). Then the reversal: "At this burn we dock two minutes short. Sludge is methane. I'm opening the crude bypass — feed me casks." Carry a cask to the **fuel intake** in engineering; the intake consumes it (+15 fuel, one cask fewer to deliver). Which cask is the player's call — the warming one that will not survive anyway is the smart burn. While the engines eat sludge the bypass mixture needle at the seat drifts; hold it in the green (the calibrate verb, minimal) or the injectors foul: Engines condition drops and a hold-E "clear the injectors" task appears. Solo, the captain calls the needle and it takes longer. *New (M3-6).*
- **12-16 Dirty burn.** Soot from the bypass adds to the hold's contaminant load (scrubbers again), the engines run Degraded, and the director keeps drawing its usual faults. With two players this is where the seat and the hold split naturally: one holds the mixture, one racks and repairs.
- **16-19 Climax.** The cargo shift (always one of the two climax incidents) lets a cask go while the fuel is marginal. Triage: rack it for pay, or burn it for margin. Solo it is a choice; crewed it is a shout.
- **19-20 Refinery approach.** Captain: "Refinery control wants the hold under 10 ppm before their people board — scrub it down." Docking card, the tally, and on the return the base's fuel line ticks up.

## 4. The tally, the pay and the base

- **Pay** = casks delivered intact × 100, plus the existing clean-manifest bonus of 150 (nothing vented, loose or burned), plus a clean-air bonus of 150 when the hold docks under 10 ppm, plus the hull bonus (mean module condition × 2, as today), minus a soot fine of 40 per cask burned and 50 per crew blackout. A clean solo run (4 intact, hull 100, clean air) pays 900; burning one cask to get home pays 610 — clearly worse, clearly survivable. (Corrected at the M3-5 red-team: the first draft forgot the manifest bonus.)
- **Fuel credit**: the refinery refuels the hauler and credits refined methane per intact cask (say 15 t per cask). What the hauler does not need for the trip home is the **surplus**, and the base fuel stock rises by it: `Base fuel 340 t (+45 this run)` under the departure board's title, next to the last-run line we already show.
- **Crew**: an exposure meter per crew member (owner-only replicated, like hold progress). It rises with the section's contaminant level, shows as a vignette and a cough, slows the walk at 60 %, and at 100 % the crew member blacks out for 30 s — the captain's med-bot, hazard pay docked — then carries on. No deaths, no hard failure: the run always docks, as the first note decided.
- **Contract tiers** replace voyage types on the board: *Milk run* (4 casks, solo-sized), *Standard* (8), *Priority* (12, the one that only really pays crewed). Same run, same seed mechanism, bigger manifests and more overlapping incidents.

## 5. What is shipped, what is new

| Beat or system | Needs | Status |
|---|---|---|
| Casks: warming, relief puffs, rupture | `ACargoCrate` with volatility, heat light, vent FX; `Vented` becomes "ruptured" in lore | Shipped (M3-4). New: a venting cask adds contaminant to its section. |
| Cold rack as the holding bay; transit clamps; the cargo shift | `ColdRack` slot, `Clamp_1..3`, `CargoShift` incident | Shipped (M3-4). New: rack cooling reads the coolant loop's status. |
| Carry, put down, place | Server-owned hands, carry socket, prompt | Shipped (M3-4). |
| Hold-E repairs, task board, captain lines, incident pacing | Ship graph, director, task board | Shipped (M3-2, M3-3). New line kind `Fuel`; new hold targets: the feed valve, the injectors. |
| Control seat and board | `AControlStation`, `ControlBoardWidget` | Shipped (level pass). New: scrubber toggle, fuel gauge, mixture needle. |
| Tally and hub board | `FVoyageResult`, `Last run` line | Shipped (M3-4). New: burned casks, clean-air bonus, fuel credit, base stock. |
| Ship atmosphere and scrubbers | `FShipAtmosphere`: contaminant ppm per section, sources (venting casks, the dirty burn), sink (scrubbers × power), per-crew exposure and effects | **New — M3-5** |
| Fuel and the crude bypass | `FFuelSystem`: quantity, burn rate, the feed valve, the bypass and its mixture; `AFuelIntake` slot that consumes a cask; injector fouling | **New — M3-6** |
| The refinery | Dock = the voyage's end plus a "REFINERY — DOCKING" card and the fuel credit; a walkable refinery dock deck is optional later | **New — M3-7** |

Starting numbers for the briefs (tune in playtest): contaminant in ppm per section; a relief puff adds 2 ppm/s for 10 s and repeats every 20 s while the cask is at or above the warn temperature; a rupture adds 5 ppm/s for 30 s; scrubbers remove 0.5 ppm/s in normal mode and 3 ppm/s in hazard mode at full power, half that in a brownout; exposure rises 1 %/s at 20 ppm and scales with ppm/20; fuel is 100 units, nominal burn 4 per minute (80 for the run, 20 reserve), the failed valve bleeds 40 units in 60 s unless closed, a cask burned is +15; the mixture needle out of the green for 5 s costs Engines 10 condition and queues an injector clear.

## 6. Re-planned milestones

- **M3-5 Hazard atmosphere + scrubbers** (Astra). Pure `FShipAtmosphere` with tests; sources and sinks as above; Life Support gains a scrub mode with a press at the module and a toggle on the control board; scrub power draw through the graph (hazard mode browns out the rack when the core is Degraded); per-crew `Exposure` with the vignette, a cough placeholder and the 30 s blackout; task board rows (`Hold air 32 ppm - scrub`, red at 50); captain lines; helpers `AirlockAir`, `AirlockScrub <Off|Normal|Hazard>`, `AirlockExpose <pct>`; proof `verify-atmosphere.ps1`.
- **M3-6 Fuel + the crude bypass** (Astra). Pure `FFuelSystem` with tests; the `Fuel` line kind and the valve hold; the fuel beat as a scripted director entry in a fixed window (minute 8-11, cause varied by seed: valve, line, short-fill at the Rig); `AFuelIntake` consuming casks through the existing carry and place path; the mixture needle at the seat (the calibrate verb at its smallest); injector fouling and its hold-E clear; soot into the atmosphere; tally changes; proof `verify-fuel.ps1`.
- **M3-7 Refinery dock + base fuel** (Claude, content plus a little code). The exit airlock becomes the refinery dock with its own card; the fuel credit and `BaseFuelStock` persisted (SaveGame in the MVP, server-side later) with the base line on the departure board; the three contract tiers instead of the three voyage types; the hub's lore reskin (signage, board title `VELL EXTRACTOR - DOCK B DEPARTURES`, the refinery's mirror field in the sky) without renaming any section; the hold's map pass (a real cryo rack fixture, the fuel intake hatch in engineering).
- **M3-8 Legibility** as planned, now including the haze, the ppm readout and the fuel gauge.
- **Deferred**: parts and cannibalize slots (optional flavour now), the power lobbying board (first note, lever 2) as M3-9, fires and the extinguisher.

The `AGENTS.md` "What the game is" line would change from "instanced ~20-minute voyages (mining, pirates, derelicts)" to "the refining run in three contract tiers" once Tom approves — a docs commit, nothing in code.

## 7. Risks and questions for Tom

- **Names**: Vell, Halder, Vell crude / sludge, the Rig, Halder Solar Refinery — placeholders to keep or replace.
- **Tone**: cyanide poisoning is grim on paper; on screen it is a vignette, a cough and a thirty-second blackout, which reads as hazard rather than gore. Say if the rating target needs it softer.
- **Predictability**: a scripted fuel beat is a set piece; after three runs players expect it. Varying its cause and minute, and keeping the director's random faults around it, should hold it for the MVP; the long-term answer is a deck of set pieces per contract tier.
- **Solo balance**: burning one of four casks is a quarter of the pay; the pay curve must keep "burn one, dock clean" above "rack all four, dock late" or solo players will never touch the bypass. The numbers in section 4 do that; playtest will move them.
- **The refinery as a place**: arriving somewhere new is a strong moment and a small dock deck would sell it. Not for the MVP: the card and the tally carry it, and a second hub map costs a level pass.
- **Realism bends** are listed in section 2; none of them is visible from inside the ship.

## 8. Next step

If the direction holds, the M3-5 brief (atmosphere + scrubbers) is written from sections 3 to 6 of this note, red-teamed as before, and handed to Astra. If only one of the two new systems can go first, the fuel beat (M3-6) is the stronger one — it is the reversal that makes the run a story — and the atmosphere can follow it.
