# Go Game Client
A simple Go game client with local LAN support and an image detection system so you can play on a physical board instead of the computer.
The whole project is still very much work in progress. Current focus lies on the image detection system as well as a robotic arm that can mirror the opponents move on the physical board.
The project is aimed to be kept modular so you can easily just take whatever parts are useful to you.

## Motivation
The motivation for this project is simple. 
I don't enjoy playing on the computer and don't have Go-interested people near me.
So let's replace the opponent with a robotic arm and play other people online but over the board.

### Goal
The final goal is to have a full robotic Go set.
We may document a parts list for the hardware and provide the software here.
A user may then purchase this hardware at the best available price and experience the fun of assembling everything.
Finally flashing this software to get access to local and online games, puzzles, and training against bots.
All open source so you can tinker around as you like.

## Components
### Internal Components
Name        | Description 
------------|------------
libCore     | Library for game rules, basic types, board state validation, etc.
libNetwork  | Library for network layer networking. Sending/Receiving messages.
libGameNet  | Library for application layer networking. Translating game events to/from messages. Uses libNetwork.
libCamera   | Library for physical board detection using OpenCV.
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

## General Documentation
- [General](docs/Documentation.md) — entry point and general notes
- [Core](Core.md) — core rules/logic overview
- [GUI](docs/GUI.md) — GUI architecture and rendering notes
- [Networking](docs/Networking.md) — higher-level networking notes

## Technical Documentation
- [libCore](src/libCore/README.md) — Core rules, game loop, deltas, and move validation
- [libNetwork](src/libNetwork/README.md) — Network layer design and implementation details
- [libGameNet](src/libGameNet/README.md) — Protocol, client/server wrappers, and session mapping

## License
Licensed under the GNU Affero General Public License v3.0 (AGPL-3.0-or-later). See `LICENSE`.

Copyright (C) 2024 Oliver Benz.
