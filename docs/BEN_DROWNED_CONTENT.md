# BEN DROWNED: content, behavior and expansion guide

This mode is an offline, deterministic horror director. It observes play, selects an eligible encounter, remembers the player's responses and revisits its story in an endless cycle. It is a simulated presence inside the game. No online language model, operating-system interference or external service is involved.

## What the catalogue contains

There are **16 encounter families**, each with **four patterns** and **four tempers**: **256 catalogue combinations**. These are combinations of shared, implemented mechanics, presentation, placement, timing and difficulty. They are not 256 independently scripted quests or 256 different enemy species.

There are **32,768 distinct compositional transmissions** built from **160 authored fragments**: 16 introductions, eight specific lines for each of 16 families, and 16 endings. Each family has 2,048 text combinations. They are not 32,768 individually authored speeches. The production-core tests enumerate all combinations, verify uniqueness, and check that every transmission fits within 175 ASCII bytes.

Each family's text deck and pattern/temper deck persist with the save. An odd-multiplier permutation visits every entry exactly once before that family's deck repeats. The director excludes the previous two encounter families from its next weighted selection and reduces the chance of the older two. Reopening a saved file does not reset these decks.

## Encounter families and their four patterns

The pattern names in the catalogue are **Glimpse**, **Footsteps**, **Circle**, and **Witness**. Their concrete objectives depend on the family:

| Family | Glimpse | Footsteps | Circle | Witness | Presentation and consequence |
|---|---|---|---|---|---|
| A voice in the file | Transmission | Transmission | Transmission | Transmission | Family-specific text and restrained atmosphere; no trial or enemy |
| The other statue | Watch him | Look away | Approach | Stand still | Human Elegy apparition, native smoke and warp sound |
| Footsteps behind yours | Retreat | Travel | Look away | Circle him | Stalking apparition; unobserved relocation where the objective permits |
| An uninvited gathering | Combat | Combat | Combat | Combat | Bounded NXT reinforcements; composition and arrival timing vary |
| The borrowed hunt | Combat | Combat | Combat | Combat | Same audited reinforcement system with hunt narrative and variation |
| A private audience | Combat | Combat | Combat | Combat | One opponent requested; variant selects supported palette/tempo |
| The waterless fog | Travel | Approach | Stand still | Circle him | Temporary fog tint and statue trial |
| An eclipse without a moon | Look away | Watch him | Retreat | Stand still | Temporary darker lighting and statue trial |
| The town underneath | Travel | Circle him | Approach | Look away | Temporary native horizontal Mirror World effect |
| A dawn that forgot you | Approach | Stand still | Travel | Watch him | A gentler interval; completing the trial can grant mercy |
| Borrowed seconds | Travel | Retreat | Circle him | Approach | Short movement trial; the actual Zelda clock is untouched |
| Do not answer the footsteps | Stand still | Watch him | Look away | Approach | Attention/stillness trial |
| Something like mercy | Approach | Stand still | Watch him | Circle him | Successful trial restores half a native heart and helps nearby bots |
| The offered hand | Watch him | Approach | Stand still | Travel | A conversation-shaped trial with a successful mercy reward |
| The shape of a promise | Retreat | Look away | Travel | Circle him | Initially gentle colors darken during the event |
| The last first meeting | Combat | Combat | Combat | Combat | Late-act-weighted encounter; bounded by the same safety and resource gates |

**Gaze** means keep the statue in the camera's forward view with line of sight. **Avert** means keep it outside that view. **Approach** means remain within its marked circle. **Orbit** accumulates angular travel around the statue. **Journey** accumulates real player travel and ignores large discontinuities. **Retreat** asks for distance. **Stillness** rewards remaining still. Each objective has a visible progress indicator.

Statues serving approach, orbit or gaze trials remain anchored so the goal remains achievable. Other statues can relocate while unobserved. Placement checks the existing NXT ground/path support and avoids important scene actors. The apparition draws the game's existing human Elegy mesh directly; it is not a real puzzle-owned Elegy actor and has no puzzle collider.

The four tempers are **Faint**, **Restless**, **Insistent**, and **Unbound**. They alter duration, apparition distance, teleport cadence, atmosphere and trial targets. Core event duration spans 12–33 seconds. The game requests at most three concurrent BEN enemies through the existing NXT encounter bridge; a Duel requests one. Missing actor/object capacity, disabled reinforcement support, unsupported locations, and similar constraints cause combat to decline safely; the world can fall back to a movement trial.

The reflected world uses the existing horizontal Mirror World implementation. It is not a newly authored upside-down Clock Town map, new collision mesh or replacement scene. Turning this effect off is supported. Temporary reflection is combined with the player's normal mirror preference without overwriting that preference.

## What the director remembers and reacts to

The director uses weighted policy rather than an unconstrained random roll:

- Lingering favors a watcher or stillness encounter; sustained movement favors pursuit and travel trials.
- Recent battles leave a 60-second active-play memory that increases duel and reckoning weight. The runtime observes the RPG framework's credited-kill counter when available.
- Low health favors mercy and bargains and blocks new hostile combat, pursuit, betrayal, mirror and movement-pressure events.
- Nearby bots increase the relevance of hunts and mercy. Bots can notice the apparition and benefit from a successful mercy trial.
- Returns to a saved game increase the watcher's interest. Area arrivals receive grace time and a short memory in encounter selection.
- Repeated kindness, mercy, defiance and silence produce different relationship states and discoveries.
- The selected intensity changes pacing and danger weight. Existing combat, safe areas, swimming, cutscenes, dialogue, menus, transitions and resource budgets restrict eligibility.

All director clocks use eligible active-play time. Menus and cutscenes do not advance them. A stalled frame cannot skip the story. Area changes receive an eight-second minimum grace interval, a detected death receives 45 seconds, and a new session receives at least 12 seconds. These are minimums: an existing longer cooldown is preserved.

With default intensity, a new event is scheduled after the previous event's duration plus 36–58 seconds of active play. Restrained adds a 52–74 second gap; Relentless adds 24–46 seconds. Low health adds another 15 seconds after an event. This deliberate breathing room keeps the haunting from obscuring every ordinary interaction.

## Story structure and persistence

A loop consists of **48 encounters**, divided into six eight-encounter acts:

1. A FILE LEFT OPEN
2. THE OTHER FOOTSTEPS
3. A KINDNESS WITH TEETH
4. THE TOWN UNDERNEATH
5. WHO OWNS THE MASK
6. YOU HAVE BEEN HERE BEFORE

Later acts increase the weight of reflection, betrayal and reckoning. The 48th encounter is a directed final meeting: an established friendship or active pact favors a Bargain; otherwise the director prefers a permitted Reckoning, then Stillness, reflection or a Watcher. Safety, an active truce and the previous-two-family exclusion still apply. If every directed option is blocked, the normal eligible weighted selection supplies a fallback. A return message marks the new beginning after the encounter, and tension partly recedes. The next loop preserves relationships, discoveries, lifetime counters and deck positions. There is no permanent exorcism or setting that secretly re-enables itself.

Normal game saves preserve the optional versioned BEN section, including its RNG state, deck cursors, session/scene/cycle counters, completed loops, responses, relationship, discoveries, cooldown and last transmission. This follows normal save semantics: progress after the last save is not secretly written elsewhere. The recent 32-entry casebook is **session history**, not a separately persistent journal. A returning session can recall the saved last transmission.

BEN does not alter the vanilla inventory, permanent quest flags, save-file identity or game clock. Disabling the mode before loading a file leaves the saved BEN memory available for a later enabled session.

## Secrets and relationship spoilers

**An uneasy friendship:** offer mercy at least twice and kindness repeatedly. Mercy gives +6 mercy and +3 trust; kindness gives +5 trust. At trust 24 or higher and mercy 12 or higher, a kindness response grants four protected upcoming encounters. With no other trust changes, two mercy responses followed by four kindness responses suffice. Successful trials can supply additional trust and shorten that route.

The protected meetings exclude combat, pursuit and betrayal. Forming the pact also ends the current BEN world event. Continued kindness can renew the four-meeting pact. It is a useful, dependable benefit; there is no guaranteed punishment for attempting friendship. Once a pact naturally expires, the ordinary mix of eligible encounters can return. Friendship does not delete the haunting or stop its loops.

Other discoveries:

- **A word below the water:** listen silently on three different encounters.
- **The hand that did not strike:** accumulate at least 20 mercy; four mercy responses reach it.
- **An argument without an ending:** accumulate at least 24 defiance; four defiant responses reach it.
- **The witness:** succeed at eight trials.
- **The return:** complete one 48-encounter loop.

Only one response is accepted per encounter, including across normal save/reload. Trial outcomes, responses and discoveries change memory, not the player's actual save ownership. Failed trials do not confiscate inventory or apply a hidden remote health penalty. Spawned enemies still participate in normal game combat.

## Extending the system

`BenCore.h` owns policy and deterministic state changes. `Context` carries values from active simulation; one-shot fields such as deaths, kills and trial results must be delivered once. `Update` returns a `Decision` only when the cooldown and eligibility gates permit it. `ForceEvent` exists for controlled testing and still enforces eligibility. The core has no engine pointers or external dependencies.

`BenContent.h` maps a family and text ID to its names, catalogue label and assembled transmission. Keep ID meanings stable when editing wording. Changing the number of families, variants or texts requires deliberate save-schema migration rather than silently changing the modulus of existing cursors.

`BenWorld.cpp` executes decisions: trial rules, owned apparition presentation, temporary atmosphere, reflection and calls into the bounded NXT encounter bridge. New mechanics belong here or in a similarly isolated adapter, not in a narrative-string switch or the render-thread UI.

`BenSaveData.h` is a pointer-free C POD appended to `ShipSaveInfo`; `BenJson.h` is its strict, transactional JSON codec. Unknown versions must not be silently rewritten. Changes to the C save structure require a complete rebuild.

Run `python3 tests/ben_drowned/run_tests.py --sanitizers --compile-engine` for the supplied headless regression workflow. Inspect its report for dependency-related tests that were not run. A native gameplay and renderer test remains necessary to assess final appearance, route accessibility and balance on real game content.
