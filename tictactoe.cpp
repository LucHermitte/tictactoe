/**
 * @file    tictactoe.cpp
 */
/**\mainpage
 * @brief Tic-Tac-Toe AI in C++11.
 *
 * This little program has been written as a solution to a tic-tac-toe
 * exercise on the French C++ forum of the siteduzero.com.
 *
 * It implements two AI strategies:
 * [negamax](http://en.wikipedia.org/wiki/Negamax), and negamax with
 * alpha/beta pruning:
 * [negascout](http://en.wikipedia.org/wiki/Negascout). It lets the user
 * the possibility to play against the machine, or two AIs to play
 * against each other.
 *
 * @note 
 * I took this exercise as an excuse to play with a few C++11 features:
 * <code>enum class</code>, tuples, rvalue-references, \c
 * std::unique_ptr<>, \em etc.
 *
 * @author Copyright 2011,2013 Luc Hermitte
 * <p><b>Licence</b> GPL v3.0.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
#include <tuple>
#include <memory>
#include <fstream>
#include <ostream>
#include <iostream>
#include <cstdlib>

#include <cassert>

#define DEBUG_AI_LEVEL 0

/**@defgroup gPlayer Player related definitions */
/**@defgroup gGame Game related definitions */

/**@defgroup gPlayerId Player Identifier
 *\ingroup gPlayer
 *@{
 */
/// Player Id.
enum class PlayerId {
    first=1, second=2
};
/// Operator to iterate over players.
PlayerId operator++(PlayerId &id, int) {
    PlayerId tmp = id;
    id=PlayerId(3-size_t(id));
    return tmp;
}
/// Displays what the player uses on the board ('X', or 'O').
std::ostream & operator<<(std::ostream & os, const PlayerId & v) {
    const char c = (v==PlayerId::first ? 'X' : 'O');
    return os << c;
}
//@}

/**@defgroup gSquares Squares from the Board
 *\ingroup gGame
 *@{
 */
/** Value of a square.
 * A square on the board may be %unoccupied, or occupied by a token of
 * any player.
 */
enum class SquareValue {
    unoccupied  =0,
    first =PlayerId::first,
    second=PlayerId::second
};

/** %Square on the \c Board.
 * This helper class holds the value of a square, and provides
 * a conversion function to display the square content.
 */
struct Square {
    /// Init constructor.
    Square(SquareValue v = SquareValue::unoccupied) : m_value(v) {}
    /// Value accessor.
    SquareValue value() const { return m_value; }
    /// Conversion function to a displayable character.
    char as_char() const {
        switch (m_value) {
            case SquareValue::first: return 'X';
            case SquareValue::second: return 'O';
            case SquareValue::unoccupied: return ' ';
        }
        assert(!"Invalid value");
    }
private:
    SquareValue m_value;
};
/// Displays the content of a square.
std::ostream& operator<<(std::ostream& os, Square c) {
    return os << c.as_char();
}
//@}

/*===========================================================================*/
/*==============================[ Coordinates ]==============================*/
/*===========================================================================*/
/**@defgroup gCoordinate Coordinates of Squares on the Board
 *\ingroup gGame
 *@{
 */
/** Coordinates definition.
 * As a tuple (row x column)
 */
typedef std::tuple<size_t, size_t>       Coords;
/** Distance vector between two Squares.
 */
typedef std::tuple<ptrdiff_t, ptrdiff_t> Delta;

/// Increments a coordinates pair.
Coords     & operator+=(Coords& c, Delta const& d) {
    std::get<0>(c) += std::get<0>(d);
    std::get<1>(c) += std::get<1>(d);
    return c;
}
Coords const operator+ (Coords  c, Delta const& d) {
    return c+=d;
}

/// Decrements a coordinates pair.
Coords     & operator-=(Coords& c, Delta const& d) {
    std::get<0>(c) -= std::get<0>(d);
    std::get<1>(c) -= std::get<1>(d);
    return c;
}
Coords const operator- (Coords  c, Delta const& d) {
    return c-=d;
}

/// Tells whether \f$0<= c_1<M_1\f$ and \f$0<=c_2<M_2\f$
bool in_range(Coords const& c, Coords const& M) {
    return std::get<0>(c) >= 0
        && std::get<1>(c) >= 0
        && std::get<0>(c) <  std::get<0>(M)
        && std::get<1>(c) <  std::get<1>(M);
}

std::ostream& operator<<(std::ostream&os, Coords const& c) {
    return os << '{' << std::get<0>(c) << ',' << std::get<1>(c) << '}';
}
//@}


/*===========================================================================*/
/*=================================[ Board ]=================================*/
/*===========================================================================*/
/**@defgroup gBoard Board related definitions
 * @ingroup gGame
 * @{
 */
/** %Board Definition.
 * A board is made of \c L_ x \c C_ \link Square squares\endlink.
 *
 * We can:
 * - sets the value of a square
 * - interrogates the value of a square
 */
struct Board
{
    /// Init constructor.
    Board(size_t L_, size_t C_)
        : m_board(L_*C_), m_L(L_), m_C(C_) {}
    /// Init constructor for square boards.
    Board(size_t L_=3)
        : m_board(L_*L_), m_L(L_), m_C(L_) {}
    // Board(Board && tmp)

    /// \c Square accessor.
    Square const& operator()(size_t l, size_t c) const {
        return board(l,c);
    }
    /// \c Square accessor.
    Square const& operator()(Coords c) const {
        return board(std::get<0>(c), std::get<1>(c));
    }

    /// Is a \c Square unoccupied ?
    bool is_empty(size_t l, size_t c) const {
        return board(l,c).value() == SquareValue::unoccupied;
    }
    /** Occupies a \c Square with a player's move.
     * @pre the square must be unoccupied.
     * @post <tt>board(l,c) == v</tt>
     * @return whether the operation has succeeded
     */
    bool set(size_t l, size_t c, SquareValue v) {
        if (board(l,c).value() != SquareValue::unoccupied) {
            return false;
        }
        board(l,c) = v;
        return true;
    }
    /** Occupies a \c Square with a player's move.
     * @pre the square must be unoccupied.
     * @post <tt>board(c) == v</tt>
     * @return whether the operation has succeeded
     */
    bool set(Coords const& c, SquareValue v) {
        return set(std::get<0>(c), std::get<1>(c), v);
    }
    /** Clears a \c Square.
     * @post <tt>board(l,c) == SquareValue::unoccupied</tt>
     */
    void reset(size_t l, size_t c) {
        board(l,c) = SquareValue::unoccupied;
    }
    /** Clears a \c Square.
     * @post <tt>board(c) == SquareValue::unoccupied</tt>
     */
    void reset(Coords const& c) {
        reset(std::get<0>(c), std::get<1>(c));
    }

    /// Maximum number of rows of the board.
    size_t L() const { return m_L; }
    /// Maximum number of columns of the board.
    size_t C() const { return m_C; }
    /// Coordinates of the bottom-right.
    Coords M() const { return Coords{L(), C()}; }
private:
    /** Internal \c Square accessor.
     * @pre <tt>l < L()</tt>, checked with an assertion
     * @pre <tt>c < C()</tt>, checked with an assertion
     */
    Square      & board(size_t l, size_t c) {
        assert(l < L());
        assert(c < C());
        return m_board[l*C()+c];
    }
    Square const& board(size_t l, size_t c) const {
        return const_cast <Board*>(this)->board(l,c);
    }
    std::vector<Square> m_board;
    size_t            m_L;
    size_t            m_C;
};

/// Helper function to draw an line of +-+-+-+...+.
std::ostream & draw_line(std::ostream& os , size_t C)
{
    os << '+';
    for (size_t c=0; c!=C ; ++c) 
        os << "-+";
    return os;
}

/// Displays a \c Board on a text stream.
std::ostream& operator<<(std::ostream&os, Board const& b) {
    draw_line(os, b.C());
    for (size_t l=0; l!=b.L() ; ++l) {
        os << "\n|";
        for (size_t c=0; c!=b.C() ; ++c) {
            os << b(l,c) << '|';
        }
        draw_line(os<<'\n', b.C());
    }
    return os << '\n';
}
//@}

/*===========================================================================*/
/*========================[ Player Decision Centres ]========================*/
/*===========================================================================*/
struct Game;

/**@addtogroup gPlayer
 *@{
 */
/**@defgroup gPlayerAI Player AI */
/**@ingroup gPlayerAI
 * Interface class for Player Decision Centres.
 * Human players, and AI players all share the same interface when
 * considering their decisions: they are asked what move they \c choose
 * to perform.
 * @note non copyable.
 */
struct PlayerDC
{
    virtual ~PlayerDC() {};
    /**
     * Chooses the next move.
     * This function is to be specialized depending on the actual kind
     * of player (humain, or various AI algorithms). This is where the
     * actual decision will be made
     * @param[in,out] g  Game current state. 
     * @return Next move chosen
     */
    virtual Coords choose(Game & g) const = 0;

protected:
    PlayerDC() {}
    PlayerDC           (PlayerDC const&) = delete;
    PlayerDC& operator=(PlayerDC const&) = delete;
};

/** Actual player class.
 * The \em strategy Design Pattern is implemented regarding how players
 * decide of their next move.
 */
struct Player
{
    /**
     * %Player init constructor.
     * @param[in] dc_  Decision centre for the player. 
     * @param[in] name_  Name of the new player.
     *
     * @pre \c dc_ is not null, checked with an assertion
     * @note \c dc_ responsibility is moved to the new \c Player
     * instance.
     */
    Player(std::unique_ptr<PlayerDC>&& dc_, std::string && name_)
        : m_dc(std::move(dc_))
        , m_name(name_)
        {
            assert(m_dc);
        }
    /** Choose the next move.
     * The actual choice is given to the exact \link PlayerDC decision
     * centre\endlink instantiated.
     * @param[in,out] g  Game current state. 
     * @return Next move chosen
     * @see \c PlayerDC::choose()
     */
    Coords choose(Game & g) const { return m_dc->choose(g); }
    /// Name accessor.
    std::string const& name() const { return m_name; }
private:
    std::unique_ptr<PlayerDC> m_dc;
    std::string               m_name;
};

/// Displays a player's name.
std::ostream & operator<<(std::ostream & os, const Player & v) {
    return os << v.name();
}
//@}


/*===========================================================================*/
/*=================================[ Game ]==================================*/
/*===========================================================================*/
/**@addtogroup gGame
 *@{
 */
/** %Game state.
 * This class aggregates all data about the current state of a game:
 * - the state of the \c Board,
 * - the list of \link Player players\endlink,
 * - the current number of moves accomplished,
 * - the number of aligned player tokens required to declare a win.
 */
struct Game
{
    /// Init constructor.
    Game(size_t L_=3, size_t C_=0, size_t nb_required_to_win = 0)
        : m_nb_moves(0)
        , m_board(L_, C_?C_:L_)
        , m_nb_required_to_win(nb_required_to_win ? nb_required_to_win : L_)
        {}

    /// Checks whether the \c Square at coordinates {l,c} is unoccupied.
    bool can_play_at(size_t l, size_t c) const {
        return m_board.is_empty(l,c);
    }
    /// Assigns a \c Square with a player token. 
    bool set(Coords c, PlayerId p) {
        return m_board.set(c,SquareValue(size_t(p)));
    }
    /// Empties a \c Square of any a player token. 
    void reset(Coords const& c) {
        m_board.reset(c);
    }

    /** Iterates over all possible moves, and applies a functor on the
     *  game state.
     * This is a special \c for_each that iterates over possible moves
     * from current game state.
     * @param[in] f  functor to apply on new game states built from each
     * possible moves.
     * @throw Whatever f may throw
     * @return as soon as \c f() returns \c true.
     */
    template <class F> void for_each_possible_move(F f) {
        bool cont = true;
        for (size_t l=0; cont && l!=m_board.L() ; ++l)
            for (size_t c=0; cont && c!=m_board.C() ; ++c)
                if(can_play_at(l,c))
                    cont = f(Coords{l,c});
    }

    /** Checks whether a given move is a winning move.
     * @param[in] c  coordinate where a new player token shall be
     * evaluated 
     * @param[in] p  id of the player to consider playing
     * @return whether player \c p wins if she plays at \c c.
     * @throw None
     */
    bool is_a_winning_move_for(Coords c, PlayerId p) const {
        const SquareValue v = SquareValue(size_t(p)) ;
        // vert
        if (check_orth<0>(c,v)) { return true; }
        // horiz
        if (check_orth<1>(c,v)) { return true; }
        // diag 1
        if (check_diag(c,v, Delta{1, 1})) { return true; }
        // diag 2
        if (check_diag(c,v, Delta{1, -1})) { return true; }
        return false;
    }

    /** Adds a new player to the game.
     * @param[in] player  New player (decision centre) to add, and takes
     * responsibility of.
     * @param[in] name    Name of the new player.
     * @pre the \c player shall not be null, checked by assertion.
     * @throw std::bad_alloc if memory is exhausted.
     */
    void push(std::unique_ptr<PlayerDC> && player, std::string && name) {
        assert(player);
        m_players.push_back(Player(std::move(player), std::move(name)));
    }

    /**
     * Game main function.
     * This function iterates until a player wins, or there is a draw.
     * @pre The number of registered players shall be 2; unchecked.
     * @post Either one player has won, or a draw has been established.
     */
    void run()
    {
        PlayerId player = m_nb_moves%2 == 0 ? PlayerId::first : PlayerId::second;
        while (m_nb_moves != L() * C()) {
            Player & p =  m_players[size_t(player)-1];
            std::cout
                <<"Moves: " << m_nb_moves
                << " ; Player " << size_t(player) << ", " << p.name() << ", ";
            Coords c = p.choose(*this);
            assert(in_range(c, board().M())); // choose() post constract
            if (set(c, player)) {
                std::cout << board();
                if (is_a_winning_move_for(c, player)) {
                    std::cout << "Player " << size_t(player) << ", " << p.name() << ", has won!\n";
                    return;
                }
                player++;
                m_nb_moves ++;
            } else {
                std::cout << "Cannot play there, try again.\n";
            }
        }
        std::cout << "Draw. Nobody wins.\n";
    }

    /// Internal Board accessor.
    Board const& board() const { return m_board; }
    /// Accessor to the number of rows in the board.
    size_t       L()     const { return m_board.L(); }
    /// Accessor to the number of columns in the board.
    size_t       C()     const { return m_board.C(); }
    /// Accessor to the dimension of the board.
    Coords       M()     const { return m_board.M(); }
private:

    /** Checks whether there a win on the row/column.
     * @tparam Dir direction searched (row or column)
     * @param[in] c  coordinates where a new token is evaluated whether
     * it is a winning token
     * @param[in] v  id of the player who own the token evaluated.
     *
     * @return whether this is a winning move horizontally (Dir==0), or
     * vertically (Dir==1).
     * @throw None.
     */
    template <size_t Dir> bool check_orth(Coords c, const SquareValue v) const
    {
        size_t nb = 1; // crt
        for (Coords t = c ; std::get<Dir>(t) > 0 ; ++nb) {
            std::get<Dir>(t)-- ;
            if ((m_board)(t).value() != v) break;
        }
        for (Coords t = c ; std::get<Dir>(t) < std::get<Dir>(m_board.M())-1 ; ++nb) {
            std::get<Dir>(t)++ ;
            if ((m_board)(t).value() != v) break;
        }
        return (nb >= m_nb_required_to_win) ;
    }

    /** Checks whether there a win on the diagonal.
     * @param[in] c  coordinates where a new token is evaluated whether
     * it is a winning token
     * @param[in] v  id of the player who own the token evaluated.
     * @param[in] d  orientation of the diagonal where to search for a
     * winning move.
     *
     * @return whether this is a winning move.
     * @throw None.
     */
    bool check_diag(Coords c, SquareValue v, Delta d) const {
        size_t nb = 1; // crt
        for (Coords t = c ; ; ++nb) {
            t -= d;
            if (!in_range(t, m_board.M()))  break; 
            if ((m_board)(t).value() != v) break;
        }
        for (Coords t = c ; ; ++nb) {
            t += d;
            if (!in_range(t, m_board.M()))  break; 
            if ((m_board)(t).value() != v) break;
        }
        return (nb >= m_nb_required_to_win) ;
    }

    /**@name Game dynamic data
     * Data that define the current state of the game.
     * They evolve during the game.
     */
    //@{
    size_t              m_nb_moves;
    Board               m_board;
    //@}
    /**@name Game static data */
    //@{
    size_t              m_nb_required_to_win;
    std::vector<Player> m_players;
    //@}

    friend std::istream & operator>>(std::istream & is,  Game & v);
};

std::istream & operator>>(std::istream & is,  Game & v)
{
    std::vector<std::string> lines;
    std::string line;
    size_t C;
    while (std::getline(is, line)) { // till eof
        if (line[0] == '|') {
            lines.push_back(line);
            C =  (line.size()-1)/2;
        } else if (line == "<<EOF") {
            break;
        }
    }
    // if (is) {
        Board b(lines.size(), C);
        v.m_nb_moves = 0;
        for (size_t l=0, L=b.L(); l!=L ; ++l) {
            for (size_t c=0,C=b.C(); c!=C ; ++c) {
                switch (lines[l][c*2+1]) {
                    case 'X':
                        b.set(l,c,SquareValue::first);
                        v.m_nb_moves++;
                        break;
                    case 'O':
                        b.set(l,c,SquareValue::second);
                        v.m_nb_moves++;
                        break;
                }
            }
        }
        // std::cout << b;
        v.m_board = std::move(b);
    // }
    return is ;
}
//@}

/*===========================================================================*/
/*========================[ Player Decision Centres ]========================*/
/*===========================================================================*/

/*===============================[ LocalPlayerDC : cin/cout ]================*/
/**@ingroup gPlayerAI
 * Player decision centre that delegates decisions to a human player
 * through text console.
 */
struct LocalPlayerDC : PlayerDC {
    virtual Coords choose(Game & g) const {
        size_t l, c;
        while (! (std::cout << "Where? (row col)" && (std::cin >> l >> c) && l<g.L() && c<g.C())) {
            if (std::cin.eof()) {
                throw std::runtime_error("\nAh ah, you gave up!");
            } else if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid numbers, try again: ";
            } else if (l>=g.L()) {
                std::cout << "line out of range [0,"<<g.L()<<"[, try again: ";
            } else if (c>=g.C()) {
                std::cout << "column out of range [0,"<<g.C()<<"[, try again: ";
            } else {
                assert(!"unexpected case");
            }
        }
        return Coords{l, c};
    }
};

/*===============================[ AIPlayerDC : negamax ]====================*/
/**@ingroup gPlayerAI
 * Player decision centre implemented with the negamax algorithm.
 * @see http://en.wikipedia.org/wiki/Negamax
 */
struct NegaMaxPlayerDC : PlayerDC
{
    NegaMaxPlayerDC(size_t depth, PlayerId id) : m_depth(depth), m_id(id) {}

    virtual Coords choose(Game & g) const {
        Coords best=g.M();
        int max = std::numeric_limits<int>::min();
        g.for_each_possible_move([&](Coords const& where) -> bool
            {
                g.set(where,this->m_id); // push the current move
                int eval = - this->negamax(g, this->m_depth, this->m_id, where);
                g.reset(where);   // pop the move
                if (eval > max) {
                    max = eval;
                    best = where;
                }
                return true; // continue
            });
        std::cout << "negamax plays at " << best << " (" << max << ")\n";
        if (max > +950)
            std::cout << "You'll loose!\n";
        else if (max < -950)
            std::cout << "You should win...\n";
        return best;
    }
private:
    int negamax(Game & g, size_t depth, PlayerId who, Coords const& current) const noexcept
    {
#if DEBUG_AI_LEVEL > 0
        const std::string indent (4*(6-depth), ' ');
        std::cout << indent << "negamax(" << current << ", " <<depth<<", "<<who
            // << ", alpha="<<alpha << ", beta= "<<beta
            << ")\n";
#if DEBUG_AI_LEVEL > 1
        std::cout << g.board();
#endif
#endif

        // terminal conditions => heuristic
        if (g.is_a_winning_move_for(current, who)) {
            // const int found = (1000) * (who==this->m_id ? 1 : -1);
            const int found = -(1000) + depth;
#if DEBUG_AI_LEVEL > 0
            std::cout << indent << "  "<<current<<"-> ... winning move => "<<found<<"("<<who<< ")\n" ;
#endif
            return found;
        } else if (depth == 0) {
#if DEBUG_AI_LEVEL > 0
            std::cout << indent << "  "<<current<<"-> ... exploration leaf => "<<0<<"("<<who<< ")\n" ;
#endif
            return 0; // should return how many openings there are
        }

        // else loop on all children nodes
        int max = std::numeric_limits<int>::min();
#if DEBUG_AI_LEVEL > 0
        Coords best=g.M();
#endif
        PlayerId adv = who; adv ++;
        g.for_each_possible_move(
            [&](Coords const& child_node) -> bool {
                g.set(child_node,adv); // push the current move
                int eval = - this->negamax(g, depth-1, adv, child_node);
                g.reset(child_node);   // pop the move
                if (eval > max) {
                    max = eval;
#if DEBUG_AI_LEVEL > 0
                    best= child_node;
#endif
                }
                return true; // continue
            });
        if (max == std::numeric_limits<int>::min()) { // no child node
            max = 0;
        }
#if DEBUG_AI_LEVEL > 0
        std::cout << indent << "  "<<current<<"-> best move="<<best<<" => "<<max<<"("<<who<< ")\n" ;
#endif
        return max;
    }

    const size_t   m_depth;
    const PlayerId m_id;
};


/*===============================[ AIPlayerDC : negamax alpha-beta ]=========*/
/**@ingroup gPlayerAI
 * Player decision centre implemented with the negamax with alplha/beta algorithm, aka negascout.
 * @see http://en.wikipedia.org/wiki/Negascout
 */
struct NegaMaxPlayerAlphaBetaDC : PlayerDC
{
    NegaMaxPlayerAlphaBetaDC(size_t depth, PlayerId id) : m_depth(depth), m_id(id) {}

    virtual Coords choose(Game & g) const {
#if DEBUG_AI_LEVEL > 0
        std::cout << "\n";
#endif
        Coords best=g.M();
        int max = std::numeric_limits<int>::min();
        int alpha = -1000;
        int beta  = +1000;
        g.for_each_possible_move([&](Coords const& where) -> bool {
                g.set(where,this->m_id); // push the current move
                int eval = - this->negamax(g, this->m_depth, this->m_id, where, -beta, -alpha);
                g.reset(where);   // pop the move
                if (eval > max) {
                    max = eval;
                    best = where;
                }
                if (eval > alpha) {
                    alpha = eval;
                    if (alpha >= beta) {
                        return false; // abort loop
                    }
                }
                return true; // continue
            });
        std::cout << "negamax plays at " << best << " (" << max << ")\n";
        if (max > +950)
            std::cout << "You'll loose!\n";
        else if (max < -950)
            std::cout << "You should win...\n";
        return best;
    }
private:
    int negamax(Game & g, size_t depth, PlayerId who, Coords const& current, int alpha, int beta) const noexcept
    {
#if DEBUG_AI_LEVEL > 0
        const std::string indent (4*(6-depth), ' ');
        std::cout << indent << "negamax(" << current << ", " <<depth<<", "<<who
            // << ", alpha="<<alpha << ", beta= "<<beta
            << ")\n";
#if DEBUG_AI_LEVEL > 1
        std::cout << g.board();
#endif
#endif

        // terminal conditions => heuristic
        if (g.is_a_winning_move_for(current, who)) {
            // const int found = (1000) * (who==this->m_id ? 1 : -1);
            const int found = -(1000) + depth;
            // todo: find a way to shorten the suffering! (+depth is not
            // enough)
#if DEBUG_AI_LEVEL > 0
            std::cout << indent << "  "<<current<<"-> ... winning move => "<<found<<"("<<who<< ")\n" ;
#endif
            return found;
        } else if (depth == 0) {
#if DEBUG_AI_LEVEL > 0
            std::cout << indent << "  "<<current<<"-> ... exploration leaf => "<<0<<"("<<who<< ")\n" ;
#endif
            return 0; // should return how many openings there are
        }

        // else loop on all children nodes
        int max = std::numeric_limits<int>::min();
#if DEBUG_AI_LEVEL > 0
        Coords best=g.M();
#endif
        PlayerId adv = who; adv ++;
        g.for_each_possible_move(
            [&](Coords const& child_node) -> bool {
                g.set(child_node,adv); // push the current move
                int eval = - this->negamax(g, depth-1, adv, child_node, -beta, -alpha);
                g.reset(child_node);   // pop the move
                if (eval > max) {
                    max = eval;
#if DEBUG_AI_LEVEL > 0
                    best= child_node;
#endif
                }
                if (eval > alpha) {
                    alpha = eval;
                    if (alpha >= beta) {
                        return false; // abort loop
                    }
                }
                return true; // continue
            });
        if (max == std::numeric_limits<int>::min()) { // no child node
            max = 0;
        }
#if DEBUG_AI_LEVEL > 0
        std::cout << indent << "  "<<current<<"-> best move="<<best<<" => "<<max<<"("<<who<< ")\n" ;
#endif
        return max;
    }

    const size_t   m_depth;
    const PlayerId m_id;
};



/*===========================================================================*/
/*=================================[ main ]==================================*/
/*===========================================================================*/
/** Program main function.
 * @param \-\-board to load a file of a game. (optional)
 * @param player1 type of player (n -> negamax, a -> negamax+alpha-beta,
 * h -> human)
 * @param player2 type of player (n -> negamax, a -> negamax+alpha-beta,
 * h -> human)
 * @return \c EXIT_SUCCES if the execution succeeded
 * @return \c EXIT_FAILURE otherwise
 */
int main (int argc, char **argv)
{
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " [options] <player> <player>"
            << "\n\t[options]"
            << "\n\t\t--board <filename>"
            << "\n\t<player>"
            << "\n\t\tn==ai player, (n)egamax"
            << "\n\t\ta==ai player, negamax-(a)lphabeta"
            << "\n\t\th==(h)uman player";
        return EXIT_FAILURE;
    }
    try
    {
        Game g(8,8,4);
        PlayerId id = PlayerId::first;
        for (int i=1; i!=argc ; ++i) {
            const std::string opt=argv[i];
            if (opt == "--board" || opt=="-b") {
                std::ifstream f(argv[++i]);
                if (!f) {
                    throw std::runtime_error("Cannot open " + std::string(argv[i]));
                }
                f >> g;
            } else if (opt == "n" || opt=="negamax") {
                g.push(std::unique_ptr<PlayerDC>(new NegaMaxPlayerDC(3, id)), "(AI-negamax)");
                id++;
            } else if (opt == "a" || opt=="negamax-ab") {
                g.push(std::unique_ptr<PlayerDC>(new NegaMaxPlayerAlphaBetaDC(5, id)), "(AI-negamax-AB)");
                id++;
            } else if (opt == "h" || opt=="human") {
                g.push(std::unique_ptr<PlayerDC>(new LocalPlayerDC()), "(Human)");
                id++;
            } else {
                g.push(std::unique_ptr<PlayerDC>(new LocalPlayerDC()), "opt");
                id++;
                // std::cerr << argv[0] << ": invalid option != i/h\n";
                // return EXIT_FAILURE;
            }
        }
        // scenario
#if 0
        g.set(0, 0, PlayerId::first);
        g.set(1, 1, PlayerId::second);
        g.set(0, 1, PlayerId::first);
        g.set(0, 2, PlayerId::second);
#endif

        std::cout << g.board();
        g.run();
    } catch (std::exception const& e) {
        std::cerr << e.what() << '\n';
    }
}
// Vim: let $CXXFLAGS='-std=c++0x -g -pedantic -Wall'
