# Network Layer (libNetwork)

This module is the low-level transport layer for the project. It is intentionally small and boring: just TCP sockets with a tiny framing header. The rest of the app (game logic, UI, etc.) treats it as a pipe.

## Big Picture

- `TcpServer` runs an accept loop on a dedicated IO thread.
- Each incoming socket becomes a `Connection`.
- `Connection` handles async read/write and calls back into `TcpServer` via lambdas.
- `TcpClient` is the synchronous client used by the game client, and also handy for tooling/tests.

## Design Choices

- **Simple framing.** We send a small header (`BasicMessageHeader`) that contains payload size, then the payload bytes. This makes parsing very direct and keeps the client/server symmetric.
- **No exceptions on the hot path.** Asio is used with `error_code` overloads so we can return false / empty on failure instead of throwing.
- **Connection lifetime safety.** `Connection` uses `shared_from_this()` and captures `self` in async handlers. This ensures the object stays alive until the handler finishes, which prevents use-after-free when a disconnect happens mid‑IO.
- **Single-threaded write serialization.** We use a strand and a write queue, so multiple calls to `send()` from different threads still serialize into a clean on-wire stream.
- **Lightweight public API.** The public headers are usage-focused and try to stay stable. All heavy details are in cpp files.

## Threading Model

- Server I/O runs in `TcpServer`’s IO thread (`io_context::run()`).
- `Connection` read/write handlers run on that IO thread and are serialized via the strand.
- Callbacks (`onConnect`, `onMessage`, `onDisconnect`) are invoked from the IO thread, so your handler should be fast or offload work.
- `TcpClient` is synchronous and intended to be called from a single thread.

## Message framing

Each message is:

1) `BasicMessageHeader` with a 32‑bit payload size (network byte order).  
2) `payload_size` bytes of payload data.

Payload size is bounded by `MAX_PAYLOAD_BYTES`. Oversized payloads are rejected and the connection is closed.

## Common Questions

- **Why shared_ptr for connections?** Because async handlers outlive the stack frame that kicked them off. Keeping a `shared_ptr` prevents a handler from touching a destroyed object.
- **Why does TcpClient::read() return empty string on error?** It’s the simplest non‑throwing API. You can also check `isConnected()` to see if the connection is still valid.
- **Can I call send() from multiple threads?** Yes. It is posted into the strand and queued.

## Where To Look

- `src/libNetwork/connection.*` for async read/write and lifetime rules.
- `src/libNetwork/tcpServer.*` for accept loop and connection ownership.
- `src/libNetwork/tcpClient.*` for the blocking client.
- `src/libNetwork/include/network/protocol.hpp` for framing constants and byte order helpers.
