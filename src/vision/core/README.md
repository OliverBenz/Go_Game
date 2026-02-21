# Game Detection Library (libCamera)

## Assumptions
To get a simple algorithm working for 1) detecting the board in an image 2) detecting the grid lines 3) detecting the stones, we work under some assumptions which may be weakened in the future.
 1) The board position in the image is fixed for a whole game (no bumping the table or moving the camera during game)
 2) User tunes the camera position and environment

The first assumption allows us to only properly detect the board one, then use this homography for the whole game.
The second assumption allows for the algorithm to not yet be hardened against lighting changes, strong angles and other factors which would make board detection more difficult.

The current goal is to provide a basic image detection algorithm which allows for a game to be captured.
An application to tune the algorithm parameters is provided (CameraTuner).