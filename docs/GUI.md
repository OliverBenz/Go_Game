# GUI Architecture
The goal is to keep the UI a simple presentation layer ontop of the go engine. User inputs are converted to game events and sent to the core. The UI receives signals to update from the core through the GameListener mechanism.

## Overview
- The Qt6 front end is a thin shell around the existing core game loop. The core `tengen::Game` still runs on its own thread and is notified of UI actions through the existing event queue.
- The board view is a Qt `QWidget` (`SdlBoardWidget`) that uses `QPainter` to render the grid and stone textures directly onto the widget.

## Responsibilities
- `tengen::ui::MainWindow` builds the Qt layout and forwards window close events to the core by posting a `ShutdownEvent`. It does not handle game state directly.
- `tengen::ui::SdlBoardWidget` converts left-clicks into `PutStoneEvent`s and listens for `GS_BoardChange` notifications to schedule repaints. It no longer requires an SDL surface.
- The board drawing itself remains inside `tengen::ui::BoardRenderer`, so renderer changes stay isolated from the rest of the UI.

## Threading and Updates
- The core game runs on a dedicated std::thread; the Qt event loop stays on the main thread.
- UI-to-core: mouse clicks are converted into queued game events (`PutStoneEvent`).
- Core-to-UI: `IGameListener::onBoardChange` is invoked on the game thread; the widget reposts the repaint onto the Qt thread via `QMetaObject::invokeMethod`.

## Extensibility
- The side tabs and footer are empty hooks—drop in history, chat, or status widgets without touching the rendering path.
- Because the renderer is now pure Qt painting logic, swapping to other Qt drawing approaches (e.g., `QGraphicsScene`) remains isolated to `BoardRenderer` and `SdlBoardWidget`.
- Keep business logic inside `tengen::Game` (or helpers) so the GUI remains a presentation layer only.
