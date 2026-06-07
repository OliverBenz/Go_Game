#include "core/sgf.hpp"

// The SGF Grammar: From https://homepages.cwi.nl/~aeb/go/misc/sgf.html
/*
An SGF file is a production of the following grammar, possibly with interspersed ignored parts such as whitespace (see Parsing, below).

  Collection     = { GameTree }
  GameTree       = "(" RootNode NodeSequence { Tail } ")"
  Tail           = "(" NodeSequence { Tail } ")"
  NodeSequence   = { Node }
  RootNode       = Node
  Node           = ";" { Property }
  Property       = PropIdent PropValue { PropValue }
  PropIdent      = UcLetter { UcLetter }
  PropValue      = "[" Value "]"
  UcLetter       = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" |
                   "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" |
                   "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
Here braces { } denote repetition (zero or more times), the vertical bar | denotes a choice, and the strings "(", ")", ";", "[", "]", "A", ..., "Z" are
constants.

That is, an SGF file (Collection) is the concatenation of zero or more GameTrees.
A GameTree is an open parenthesis, followed by a NodeSequence, followed by zero or more GameTrees, followed by a close parenthesis.
A NodeSequence is the concatenation of zero or more Nodes.
A Node is a semicolon followed by a Property.
A Property is an identifier (PropIdent) followed by one or more PropValues.
A PropIdent is a sequence of one or more upper case letters (UcLetters).
A PropValue consists of arbitrary data enclosed in square brackets.
*/

namespace tengen {

bool saveGameAsSgf(std::filesystem::path sgfPath, const GamePosition& position) {
}

} // namespace tengen
