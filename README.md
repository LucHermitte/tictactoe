tictactoe
=========

This little program has been written as a solution to a tic-tac-toe exercise on the French C++ forum of the siteduzero.com.

It implements two AI strategies:
[negamax](http://en.wikipedia.org/wiki/Negamax), and negamax with
alpha/beta pruning:
[negascout](http://en.wikipedia.org/wiki/Negascout). It lets the user the
possibility to play against the machine, or two AIs to play against each other.

Note
---------------
I took this exercise as an excuse to play with a few C++11 features:
_enum class_, _tuples_, _rvalue-references_, `std::unique_ptr<>`, _etc_.

The code has been documented with doxygen, just install doxygen (and dot), and
run doxygen where the `Doxyfile` is.

Compilation
---------------
With Gnumake: 

    CXXFLAGS='-std=c++0x -O2 -pedantic -Wall' make tictactoe

Licence, GPL v3.0
---------------
Copyright 2011,2013 Luc Hermitte

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
