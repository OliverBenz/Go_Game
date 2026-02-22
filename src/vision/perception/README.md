# Go Camera
This is the interface library connecting the vision::core OpenCV board detection code with the Go game we work on.
This means:
 - The vision::core returns a vector of stone values. Here, we translate this to a Board state.
 - The physical board - detected by the libCamera - is invariant under $D_4$ symmetry transformations. Once a stone (or two stones if the first is in center) is placed, the symmetry is broken and we can map to our expected board coordinates.
 - Here, we keep OpenCV a private dependency and do not propagate further.

We do not merge this library with the vision::core to keep vision::core useful to other people who do not want our data structures imposed.
This also allows us to frequently update/improve or replace the vision::core without affecting higher-level projects.

Summary:
 - The vision::core provides a general image detection algorithm of a Go board.
 - This project handles mapping of degenerate image data to model::Board states


## Design Choices
## Callback functions vs Event Handler.
Event Handler (like in the game::core): Signals updates to observers, then observers query new data from the signaling entity.
I prefer an event handler pattern for larger numbers of signals and if the signaling entity is a source of truth.

There are few events that occur in the image detection loop:
 - New stone detected
 - Board lost
 - Grid lost
and vision signals are flakey.
They should be seen as "hey, i might have found something" and not as a "there is a definite change of state".

Thus we are working with callback functions here.
