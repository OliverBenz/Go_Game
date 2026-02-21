# Go Camera
This is the interface library connecting the libCamera OpenCV board detection code with the Go game we work on.
This means:
 - The libCamera returns a vector of stone values. Here, we translate this to a Board state.
 - The physical board - detected by the libCamera - is invariant under $D_4$ symmetry transformations. Once a stone (or two stones if the first is in center) is placed, the symmetry is broken and we can map to our expected board coordinates.
 - Here, we keep OpenCV a private dependency and do not propagate further.

We do not merge this library with the libCamera to keep libCamera useful to other people who do not want our data structures imposed.
This also allows us to frequently update/improve or replace the libCamera without affecting higher-level projects.

Summary:
 - The libCamera provides a general image detection algorithm of a Go board.
 - This project provides the image detection interface required for our game.
