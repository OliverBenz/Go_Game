# Resources

This is a collection of real go board images together with extra manually extracted data for testing and training.
Every image has two companion files of the same base name:

| File          | Content                                            | Example                    |
|---------------|-----------------------------------------------------|-----------------------------|
| `<name>.txt`  | Board state (stones), in `dotBW` format              | `example.txt`               |
| `<name>.json` | Board geometry, manually labeled from the raw photo | `example.geometry.json`     |

e.g. `move_3.png` is described by `move_3.txt` and `move_3.json`.

## `dotBW` format (`.txt`)

Whitespace is ignored. `.` is an empty intersection, `B` is a black stone, `W` is a white stone.
Each line is one board row; the number of characters per line defines the board size (9/13/19).
See [`core/serializer.hpp`](../../../src/game/core/include/core/serializer.hpp) for the parser and `example.txt` for a filled-in 19x19 board.

## Geometry format (`.json`)

All pixel coordinates below are given in the **original, unrectified photo** — not in any warped/rectified output. See `example.geometry.json`.

| Field          | Meaning                                                                                     |
|----------------|-----------------------------------------------------------------------------------------------|
| `boardSize`    | Number of intersections per side (9, 13 or 19). Cross-checks the detected `BoardGeometry::boardSize`. |
| `boardCorners` | The 4 corners of the physical board's outer edge. Validates the coarse `BoardFinder` stage ($H_0$, see `src/vision/core/README.md`). |
| `gridCorners`  | The 4 outermost grid-line intersections, inset from the board edge by the frame margin. Validates the refined `GridFinder` stage ($H_1$). |

`boardCorners` and `gridCorners` are each a list of 4 `[x, y]` points, in **no particular order**: a board photographed at a strong angle has no well-defined "top-left" corner, so tests match points by nearest distance rather than by array position.
