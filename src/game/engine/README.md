# Game Engine Library (`gameEngine`)

This module defines the seam between the local rules engine and an external bot backend.
The design rule is strict: the local `gameCore` remains authoritative, and any engine only suggests moves and mirrors already accepted ones.

## Big Picture

- **`IEngineSession`**: backend-neutral contract for one bot game.
- **`KataGoSession`**: first adapter, targeting KataGo's GTP surface.
- **`gtp.*`**: small protocol helpers for coordinates, commands, and response parsing.

## Design Choices

- **Local rules stay in charge**: bot backends never mutate `Game` directly.
- **Search uses `kata-search`**: avoids KataGo changing its own board before local validation completes.
- **Transport deferred on purpose**: the stable interface lands first; subprocess complexity comes later behind the same seam.

## Where To Look

- `src/game/engine/include/engine/IEngineSession.hpp`
- `src/game/engine/include/engine/katagoSession.hpp`
- `src/game/engine/include/engine/gtp.hpp`
