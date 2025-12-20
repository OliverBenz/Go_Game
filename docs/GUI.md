# GUI Architecture
The goal is to keep the UI a simple presentation layer ontop of the go engine. User inputs are converted to game events and sent to the core. The UI receives signals to update from the core through the GameListener mechanism.

## Overview
- The Qt6 front end is a thin shell around the existing core game loop. The core `go::Game` still runs on its own thread and is notified of UI actions through the existing event queue.
- The SDL-based board renderer is embedded into a Qt `QMainWindow` via `SdlBoardWidget`, keeping the rendering code unchanged while letting Qt own the surrounding window.

## Responsibilities
- `go::ui::MainWindow` builds the Qt layout and forwards window close events to the core by posting a `ShutdownEvent`. It does not handle game state directly.
- `go::ui::SdlBoardWidget` owns the SDL window and renderer created from the Qt native surface. It converts left-clicks into `PutStoneEvent`s and listens for `onBoardChange` notifications to redraw the board.
- The board drawing itself remains inside `go::sdl::BoardRenderer`, so renderer changes stay isolated from Qt.

## Threading and Updates
- The core game runs on a dedicated std::thread; the Qt event loop stays on the main thread.
- UI-to-core: mouse clicks are converted into queued game events (`PutStoneEvent`).
- Core-to-UI: `IGameListener::onBoardChange` is invoked on the game thread; the widget reposts the repaint onto the Qt thread via `QMetaObject::invokeMethod`.

## Extensibility
- The side tabs and footer are empty hooks—drop in history, chat, or status widgets without touching the rendering path.
- Because the renderer uses `SDL_CreateWindowFrom`, replacing the board view with another widget or scene can happen without changing the rest of the window structure.
- Keep business logic inside `go::Game` (or helpers) so the GUI remains a presentation layer only.
