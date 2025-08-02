# Go Game Client


## Components

### Internal Components

Name        | Description 
------------|------------
Core        | Library for game rules, basic types, board state validation, etc.
Networking  | Library for networking related functionality. Sending/Receiving game information and synchronizing multiplayer games.
Game        | Library for game state handling. e.g. GameMode: [Local 1v1, Local Bots, Multiplayer, etc].
GameTerm    | Executable defining the terminal renderer. Inherits and uses the Game library.
GameGUI     | Executable defining the gui renderer with SDL2. Inherits and uses the Game library.

Including a [ComponentName].GTest project for each component.

The executables only specify IO handling and communicate with the Game library.
e.g. GameGUI will render information from the Game library to the screen and translation mouse/keyboard inputs before passing it directly to the Game library.


### External Components
Name   | Description
-------|------------
CMake  | Collection of CMake files.
Logger | Library for logging functionality.
Asio   | Library for networking support.
GTest  | Google unit testing library.


## Networking
For simple remote 2 player sessions, the data is sent in format
"0042BAA1690987654"
- The first 4 digits being the move number [0,9999]
- The next 1 char for the player (B=Black, W=White)
- The next 2 chars for board position (as in sgf format)
- The next 10 chars: UNIX timestamp in seconds.

So the above example would be Move: 42, Player: Black, Position: AA (A1), Timestamp: ...