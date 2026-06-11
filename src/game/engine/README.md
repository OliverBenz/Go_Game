# Wrapper around KataGo engine
Implements child process handling code + a KataGo GTP implementation (Go Text Protocol).
For the future, once we want to deal with game analysis, we can add a KataGo Json implementation.
KataGo is handled in a child process which should be available for windows + posix systems (we mostly care about linux though).



## Sources
### [1] Beej's Guide to Interprocess Communication
**Brian “Beej Jorgensen” Hall**  
Version 1.5.5 · April 18, 2026  
Copyright © 2026  
https://beej.us/guide/bgipc/html/
