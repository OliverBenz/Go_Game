# KataGo Integration

This document explains the current bot integration in Tengen-Go end to end.
It is intentionally source-oriented: after reading it, you should be able to
understand how the GUI, runtime, and engine layers cooperate without first
opening the implementation files.

## Scope

Relevant code lives in:

- `src/apps/tengen`
- `src/game/runtime`
- `src/game/engine`
- `tests/game/engine.*`
- `tests/game/runtime.*`

The integration does **not** change the core rules engine in `src/game/core`.
The local `Game` object remains authoritative for legality, captures, ko, pass,
and resign handling.

## Design Rule

The bot is a move suggestion backend, not a rules authority.

The intended ownership chain is:

`Qt GUI -> Presenter -> BotSession -> IEngineSession -> KataGo`

The important consequence is:

1. A move is first accepted by local `Game`.
2. Only accepted moves are mirrored into KataGo.
3. KataGo suggestions are pushed back into local `Game`.
4. Local `Game` decides whether the suggestion is legal.

That keeps one source of truth for board state: `gameCore`.

## Module Map

### GUI layer: `src/apps/tengen`

- `MainWindow`
  - Adds the `New Bot Game` menu action.
  - Opens `BotDialog`.
  - Emits `botGameRequested(boardSize, difficultyIndex, humanPlaysBlack)`.
- `BotDialog`
  - Collects board size, difficulty, and player color.
- `MainWindowPresenter`
  - Translates GUI choices into engine/runtime config.
  - Creates `engine::KataGoSession`.
  - Creates `app::BotSession`.
  - Owns the active `IGameSession` and `GamePresenter`.
- `GamePresenter`
  - Bridges app signals into the `GameWidget`.
- `BoardPresenter`
  - Bridges board updates and click/pass/resign actions.

### Runtime layer: `src/game/runtime`

- `BotSession`
  - Implements `IGameSession`.
  - Owns a local `Game`.
  - Owns an `IEngineSession`.
  - Mirrors accepted moves into the engine.
  - Requests bot moves on a dedicated worker thread.
- `Position`
  - Applies `GameDelta` snapshots from `Game` to a UI-facing board state.
- `EventHub`
  - Publishes app-level signals to presenters.

### Engine layer: `src/game/engine`

- `types.hpp`
  - Engine-neutral DTOs such as `GameConfig`, `Move`, `Decision`, `SearchLimits`.
- `IEngineSession`
  - Backend-neutral contract for one bot game.
- `gtp.*`
  - GTP coordinate conversion, command formatting, and response parsing.
- `KataGoSession`
  - POSIX-only KataGo adapter.
  - Launches `katago gtp`.
  - Serializes command/response traffic through a subprocess wrapper.

## Source Reading Order

Read these files in this order if you want to follow the integration from top to
bottom:

1. `src/apps/tengen/MainWindowPresenter.cpp`
2. `src/game/runtime/include/tengen/botSession.hpp`
3. `src/game/runtime/botSession.cpp`
4. `src/game/engine/include/engine/IEngineSession.hpp`
5. `src/game/engine/include/engine/types.hpp`
6. `src/game/engine/gtp.cpp`
7. `src/game/engine/katagoSession.cpp`
8. `src/game/core/game.cpp`

## Startup Flow

### GUI entry point

`MainWindow::openBotDialog()` creates `BotDialog` and emits
`botGameRequested(...)` when the user accepts.

`MainWindowPresenter::onBotGameRequested(...)` then:

1. Shuts down any existing session.
2. Validates build-time KataGo paths.
3. Constructs `engine::KataGoSession`.
4. Builds an `engine::GameConfig`.
5. Calls `engine->newGame(config)` as a preflight check.
6. Constructs `app::BotSession` with the same logical settings.
7. Constructs `GamePresenter`.

### Runtime construction

`BotSession` then:

1. Initializes local `Position`.
2. Subscribes itself to local `Game` state deltas.
3. Starts `m_gameThread`, which runs `Game::run()`.
4. Starts `m_botThread`, which runs `botLoop()`.
5. Calls `m_engine->newGame(toEngineConfig())`.
6. If the bot is Black, queues an immediate bot search.

Important current detail:

- The presenter preflights `KataGoSession::newGame(...)`.
- `BotSession` calls `newGame(...)` again on the same engine instance.

That means the engine is currently initialized twice for one bot game.

## Threading Model

There are up to four relevant threads in a bot game:

### 1. Qt UI thread

Responsible for:

- Menus and dialogs.
- `MainWindowPresenter`.
- `GamePresenter` and `BoardPresenter`.
- Painting and user input in `GameWidget` and `BoardWidget`.

Presenters never update widgets directly from non-UI threads. They use
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)` when reacting to app
signals.

### 2. Game thread: `BotSession::m_gameThread`

This thread runs `Game::run()`, which blocks on the internal event queue and
applies `PutStoneEvent`, `PassEvent`, `ResignEvent`, and `ShutdownEvent`.

When a move is accepted, `Game` synchronously emits a `GameDelta` to its state
listeners. `BotSession::onGameDelta(...)` therefore runs on the game thread.

### 3. Bot worker thread: `BotSession::m_botThread`

This thread waits on `m_botCv` until `queueBotMove()` marks
`m_botMovePending = true`.

It then:

1. Calls `m_engine->requestMove(botPlayer())`.
2. Converts the `engine::Decision` into local events.
3. Pushes the resulting move back into `Game`.

The worker thread exists so bot search does not block the rules loop or the UI.

### 4. KataGo stderr thread: `KataGoProcess::m_stderrThread`

`KataGoSession` continuously drains the child process stderr pipe so KataGo
cannot block on a full error/log stream. The last 8 KiB are retained for error
reporting.

## Synchronization

### `BotSession`

- `m_stateMutex`
  - Protects UI-facing position state and move history.
- `m_botMutex` + `m_botCv`
  - Coordinate the bot worker thread.
- `m_engineAvailable`
  - Atomic flag used to stop future engine work after a failure.

### `KataGoSession`

- `Implementation::m_mutex`
  - Serializes all public API calls.
  - Protects session state, last error, and subprocess I/O sequencing.

### Core event hubs

Both core and app event hubs are synchronous. Listener callbacks run on the
caller thread.

That means:

- `BotSession::onGameDelta(...)` runs on the core game thread.
- `BoardPresenter::onAppEvent(...)` and `GamePresenter::onAppEvent(...)` are
  triggered by runtime event publication and must remain non-blocking.

## Data Types Crossing the Layers

### Runtime-facing game state

`Game` emits `GameDelta`:

- move id
- action (`Place`, `Pass`, `Resign`)
- player
- optional coordinate
- captures
- next player
- whether the game is still active

`BotSession` applies this delta to its own `Position`, then mirrors the move to
KataGo if the engine is still available.

### Engine-facing state

`engine::GameConfig` contains:

- board size
- komi
- rules string
- generic difficulty enum
- optional generic search limits

`engine::Move` represents a mirrored or suggested move:

- `MoveKind::Place`
- `MoveKind::Pass`
- `MoveKind::Resign`

`engine::Decision` wraps the move plus optional metadata. The current
`KataGoSession` implementation only fills:

- `move`
- `rawPayload`

It does not yet parse visits or winrate out of the response.

## GTP Communication

### Transport

`KataGoSession` uses a local child process, not a shell command.

Current POSIX launch path:

1. Create stdin/stdout/stderr pipes.
2. `fork()`.
3. In the child:
   - `dup2()` stdin/stdout/stderr onto the pipes.
   - `execv()` the KataGo binary with argv.
4. In the parent:
   - Wrap the fds with `fdopen()`.
   - Start the stderr drain thread.

The executable argv is built from `KataGoLaunchConfig`:

- `executablePath`
- `gtp`
- `-config <configPath>`
- `-model <modelPath>`
- optional `-human-model <humanModelPath>`
- optional `extraArgs`

### Session setup commands

After launch, `KataGoSession::newGame(...)` sends these GTP commands:

1. `boardsize <N>`
2. `clear_board`
3. `komi <value>`
4. `kata-set-rules <rules>` if rules is non-empty
5. Zero or more `kata-set-param <name> <value>` difficulty/search commands

Current difficulty mapping:

- `Easy` -> `kata-set-param maxVisits 64`
- `Medium` -> `kata-set-param maxVisits 256`
- `Hard` -> `kata-set-param maxVisits 1024`
- `Custom` -> no default difficulty command

Optional generic limits override the defaults:

- `maxVisits`
- `maxTime`

### Mirroring accepted moves

After `Game` accepts a move, `BotSession::onGameDelta(...)` converts it into an
engine move and calls `recordMove(...)`.

Current GTP mapping:

- place -> `play <B|W> <vertex>`
- pass -> `play <B|W> pass`
- resign -> no GTP command is sent

### Requesting a bot move

`BotSession::botLoop()` asks the engine for a move using:

- `kata-search <B|W>`

This is the correct high-level choice for the current architecture because
`kata-search` does not mutate KataGo's board state. Only after local acceptance
does Tengen mirror the chosen move back with `play`.

### GTP response parsing

The engine side expects normal GTP response blocks:

- success starts with `=`
- error starts with `?`
- the block ends with a blank line

`gtp::parseResponseBlock(...)`:

- trims whitespace
- removes the leading `=` or `?`
- strips an optional numeric command id

`gtp::parseMoveResponse(...)` accepts these payload shapes:

- `<vertex>`
- `pass`
- `resign`
- `play <vertex/pass/resign>`

### Coordinate mapping

Internal board coordinates use `Coord{x, y}` with `y = 0` at the top.

GTP vertices are converted by:

- columns: `A B C ... H J ...` (`I` is skipped)
- rows: `boardSize - y`

Examples:

- internal `(0,0)` on 9x9 -> `A9`
- internal `(8,8)` on 9x9 -> `J1`

## Move Flow

### Human move

1. User clicks the board.
2. `BoardPresenter::onBoardEvent(...)` calls `IGameSession::tryPlace(...)`.
3. `BotSession::tryPlace(...)` verifies it is the human turn.
4. `BotSession` pushes `PutStoneEvent` into local `Game`.
5. `Game` validates legality and emits `GameDelta`.
6. `BotSession::onGameDelta(...)`
   - updates local `Position`
   - mirrors the accepted move to KataGo
   - emits app signals for board/player/state
   - queues a bot move if the next player is the bot

### Bot move

1. `queueBotMove()` wakes the bot worker thread.
2. `botLoop()` calls `requestMove(botPlayer())`.
3. `KataGoSession` sends `kata-search`.
4. The chosen move is parsed into `engine::Decision`.
5. `BotSession::applyEngineDecision(...)` pushes the suggested move into local
   `Game`.
6. `Game` validates the move and emits another `GameDelta`.
7. `BotSession::onGameDelta(...)` mirrors that now-accepted bot move back into
   KataGo using `play`.

The bot move is therefore mirrored into KataGo only after local acceptance.

## Error Handling

### `KataGoSession`

Errors are stored in `m_lastError` and move the session into
`SessionState::Error`.

Error messages attempt to include:

- a high-level prefix
- the last GTP command sent
- the process/stdout failure detail
- the stderr tail

### `BotSession`

`BotSession` does not currently surface engine failures through a dedicated app
signal or presenter callback. It only:

- logs the error
- sets `m_engineAvailable = false`

This means engine failures are currently handled as an internal runtime concern,
not as a GUI-visible session state.

## Build and Runtime Configuration

The GUI and the integration tests both discover KataGo artifacts relative to
the source tree:

- executable: `../KataGo/cpp/katago`
- config: `../KataGo/cpp/configs/gtp_example.cfg`
- model: first non-`human` `*.bin.gz` or `*.txt.gz`

In the GUI app this is compiled into the binary using
`target_compile_definitions(...)` in `src/apps/tengen/CMakeLists.txt`.

`KataGoLaunchConfig` itself is more flexible than the GUI wiring and supports:

- custom executable path
- custom config path
- custom model path
- optional human model path
- arbitrary extra argv

## Tests

### Unit tests

- `tests/game/engine.unit/gtp.gtest.cpp`
  - coordinate conversion
  - GTP response parsing
  - move parsing
- `tests/game/runtime.unit/botSession.gtest.cpp`
  - verifies that local and bot moves both flow through core `Game`

### Integration tests

- `tests/game/engine.integration/katago.gtest.cpp`
  - launches `KataGoSession` directly
- `tests/game/runtime.integration/botSession.gtest.cpp`
  - launches `BotSession` with a real `KataGoSession`

Both integration suites depend on a working local KataGo installation and a
compatible runtime environment for the selected config/model.

## Current Caveats

These are part of the present implementation and are important for anyone
reading the code:

1. `MainWindowPresenter` preflights `engine->newGame(...)`, and `BotSession`
   initializes the same engine again.
2. Engine failures are logged but are not modeled as a user-visible session
   state.
3. `BotSession::shutdown()` joins the bot worker before calling
   `m_engine->shutdown()`, so an in-flight blocking search is not proactively
   interrupted.
4. GUI/runtime configuration is still tightly coupled to a local
   `../KataGo/cpp` checkout.
5. `KataGoSession` is POSIX-only.

## Summary

The current KataGo integration has the right architectural boundary:

- local `Game` owns legality
- `BotSession` owns orchestration
- `IEngineSession` isolates the backend
- `KataGoSession` owns transport and GTP details

The most important implementation details to keep in mind are the thread split,
the mirror-after-accept rule, and the fact that the current GUI startup path
initializes the same engine session twice.
