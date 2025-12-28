# Networking

## Planning
We want a TCP server that accepts two clients. Player Black and White.
The server is authoritative keeping track of the core game and accepting/rejecting moves and acting as a single source of truth on the game state.

Components: 
- 1 Server    (Central game controller)
- 2 Clients   (Black / White)
- N Observers (Spectators)

Clients send intent, Server sends facts

Advantages:
- No desyncs
- Observers are trivial
- Clean rule enforcement
- Deterministic
- Easy to debug

The Clients are effectively view to the server state.
Clients do not enforce events, their signals to the server are only attempts:
- AttemptPutStone { Coord c; }
- AttemptPass {}
- AttemptResign {}

Messaging in normal game:
- Client A tries to send single event to server
- Server confirms event for A
- Server sends game update event to B and all N observers
- Client A applies server confirmed event
- Client B and Observers apply game update event


Messaging in failure recovery/join/reconnct events:
- Clients get full game state (player active, player client, full board, time left, move number, hash, etc.)

Low overhead messaging
- Ideally fixed length messages for game events
- Variable length messages for chat, spectators, reconnect snapshots, etc.


## Implementation Phases
Phase 1: TCP Implementation (turn-based Go)
- single connection per client
- length-prefixed messages
- server authoritative state
- per-connection read buffer + frame parser
- heartbeat + timeout

Phase 2: Async server with many clients
- async_accept
- async_read into a ring buffer / vector
- per-client session objects
- message queue into game thread


## Code ideas
``` c++
enum class NetMsgType : uint8_t {
    AttemptPutStone,
    Pass,
    Resign,
    StateSync,
    // ...
};

struct PutStonePayload {
    uint16_t x;
    uint16_t y;
};

struct PassPayload {
    uint8_t dummy;
};

struct ResignPayload {
    uint8_t dummy;
};

struct StateSyncPayload {
    uint16_t moveCount;
    // or an index into a separate transfer
};

struct NetMessage {
    NetMsgType type;
    uint8_t    player;   // optional, depending on direction

    union {
        PutStonePayload putStone;
        PassPayload     pass;
        ResignPayload   resign;
        StateSyncPayload sync;
    } payload;
};
```