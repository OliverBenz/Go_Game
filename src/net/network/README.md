# GameNet Layer (libGameNet)

This module sits between the raw TCP transport (libNetwork) and the game logic.
It turns typed game events into wire messages and back, and it manages server/client roles.

## Big Picture

- **Client**: `network::Client` wraps `network::TcpClient` and runs a read thread.
- **Server**: `network::Server` wraps `network::TcpServer` and exposes a clean event callback.
- **Events**: All wire messages are defined in `nwEvents.hpp` and serialized as JSON.
- **Sessions**: `SessionManager` maps `ConnectionId` <-> `SessionId` and tracks seats.

## Design Choices

- **Small protocol**: JSON is used for clarity and quick iteration. May be improved in the future.
- **Thread split**: server processing is on its own thread; client has a blocking read thread.

## Where To Look

- `src/libGameNet/include/gameNet/nwEvents.hpp` for the protocol types.
- `src/libGameNet/nwEvents.cpp` for serialization/parsing.
- `src/libGameNet/server.*` for the server wrapper and event forwarding.
- `src/libGameNet/client.*` for the client wrapper and read loop.
