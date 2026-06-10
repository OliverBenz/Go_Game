# Engine Integration Plan

## Goal

Add a bot game mode without letting the external engine leak into the GUI or the core rules engine.
The local `Game` stays authoritative. KataGo is only a move suggestion backend.

## Chosen Seam

1. `app::BotSession` implements the same `IGameSession` contract the presenters already know.
2. `engine::IEngineSession` is the backend seam under `BotSession`.
3. `engine::KataGoSession` is the first implementation of that seam.

This keeps the swap boundary small:

`Widget -> Presenter -> BotSession -> IEngineSession -> KataGo`

## Why GTP First

For interactive play, KataGo's GTP mode is the right first transport:

- It is built for move-by-move play.
- It lets us set rules, board size, komi, and search params dynamically.
- We can use `kata-search` instead of `genmove`, which means KataGo suggests a move without mutating its own board.

That last point matters. The intended flow is:

1. Local player move is accepted by `gameCore`.
2. `BotSession` mirrors the accepted move into the engine with `recordMove`.
3. When it is the bot's turn, `BotSession` asks the engine for a suggestion.
4. The suggested move is pushed back into local `gameCore`.
5. Only after local acceptance do we mirror that bot move into KataGo.

So there is only one source of truth: local rules.

## Module Layout

- `src/game/engine`
  - `IEngineSession`
  - engine-neutral DTOs
  - GTP helpers
  - `KataGoSession`
- `src/game/runtime`
  - `BotSession`

## Difficulty Strategy

Keep difficulty generic at the app boundary.
The current seam uses `engine::Difficulty` plus optional generic search limits.

KataGo then maps those to its own settings internally, for example:

- `Easy` -> lower `maxVisits`
- `Medium` -> moderate `maxVisits`
- `Hard` -> higher `maxVisits`
- `Custom` -> explicit limits

That avoids baking KataGo-specific params into presenters or widgets.

## What Is Intentionally Missing

The subprocess transport is still a skeleton.
That is deliberate because the unsafe part is not the API shape, it is the lifetime and I/O management.

The real implementation should add:

1. A child-process wrapper with argv-based launch, no shell.
2. Dedicated stdout parsing for full GTP response blocks.
3. Continuous stderr draining so KataGo cannot deadlock on a full pipe.
4. A single serialized command lane.
5. A dedicated worker thread for engine search, so bot thinking never blocks the rules loop.
6. Replay-based resync if engine and local state ever diverge.

## Next Step

Implement the KataGo transport behind `KataGoSession` without changing the public interfaces that were added here.
