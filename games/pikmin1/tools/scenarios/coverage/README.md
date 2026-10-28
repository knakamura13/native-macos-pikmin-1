# Scenario coverage inventory

Static read of the game code (nothing was run) listing what a scenario could cover, split into three slices. Each table row has the trigger with code pointers, the observable assertions, the tier (stand-in assets in CI, or a real install locally), how a scenario would drive it, effort, regression risk and existing coverage. Line numbers drift; function names are the stable pointers.

- [terminals.md](terminals.md): day end, extinction, captain death, endings, exit and save flows, and the port's mode terminals.
- [milestones.md](milestones.md): first-of events and progression (Onion unlocks, ship parts, stage unlocks, demos, text, achievements).
- [edge-cases.md](edge-cases.md): counts and caps, Pikmin and captain state machines, hazards, whistle and plate, settings interactions, determinism, plus five invariant checkers that could run in every scenario.

Tracked in the fork issue "Scenario coverage plan". Written 2026-10-28 against `kyle-develop` rebuild v1 plus PR #73.
