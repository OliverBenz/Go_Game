# Core Library (libCore)

This module is the rules engine. It owns the game loop, validates moves, and emits deltas.
External code should treat it like a black box: push events in, listen to deltas out.
A delta is a set of game state changes since the last move.

## Big Picture

- **Game**: owns the rules loop and emits `GameDelta` updates.
- **MoveChecker**: stateless rule checks (suicide, captures, superko).
- **Position/Board**: lightweight state containers used by the rules engine.
- **EventHub**: synchronous sending of signals to listeners.

## Happy Path

1) External code pushes a `GameEvent` (put/pass/resign).
2) Game validates the move (including superko).
3) Game mutates internal state and emits `GameDelta`.
4) Listeners rebuild their own view of state from deltas.

This ensures all game logic is modularized and external components are pure representations of the board.

## Design Choices

- **Deltas are the source of truth**: callers do not query internal state.
- **Single‑threaded rules**: Game is designed to run its loop on one thread.
- **Deterministic hashing**: Zobrist hash is seeded for reproducibility.

## Where To Look

- `src/libCore/game.*` for the rules loop and delta emission.
- `src/libCore/moveChecker.*` for legality and capture logic.
- `src/libCore/board.*` and `src/libCore/position.*` for data structures.
- `src/libCore/zobristHash.hpp` for hash generation.
