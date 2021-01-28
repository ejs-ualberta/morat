
#pragma once

//Interface for the various agents: players and solvers

#include "../lib/outcome.h"
#include "../lib/sgf.h"
#include "../lib/types.h"

#include "board.h"


namespace Morat {
namespace Y {

class Agent {
protected:
	typedef std::vector<Move> vecmove;
public:
	Agent() = delete;
	Agent(const Board & b) : rootboard(b) { }
	virtual ~Agent() { }

	virtual void search(double time, uint64_t maxruns, int verbose) = 0;
	virtual Move return_move(int verbose) const = 0;
	virtual void set_board(const Board & board, bool clear = true) = 0;
	virtual void move(const Move & m) = 0;
	virtual void set_memlimit(uint64_t lim) = 0; // in bytes
	virtual void clear_mem() = 0;

	        vecmove get_pv() const { return get_pv(vecmove()); }
	virtual vecmove get_pv(const vecmove& moves) const = 0;
	        std::string move_stats() const { return move_stats(vecmove()); }
	virtual std::string move_stats(const vecmove& moves) const = 0;
	virtual double gamelen() const = 0;

	virtual void timedout(){ timeout = true; }

	virtual void gen_sgf(SGFPrinter<Move> & sgf, int limit) const = 0;
	virtual void load_sgf(SGFParser<Move> & sgf) = 0;
        Outcome root_outcome = Outcome::UNKNOWN;

protected:
	volatile bool timeout;
	Board rootboard;

	static Outcome solve1ply(const Board & board, unsigned int & nodes) {
		Outcome outcome = Outcome::UNKNOWN;
		Side turn = board.to_play();
		for (auto move : board) {
			++nodes;
			Outcome won = board.test_outcome(move, turn);

			if(won == +turn)
				return won;
			if(won == Outcome::DRAW)
				outcome = Outcome::DRAW;
		}
		return outcome;
	}

	static Outcome solve2ply(const Board & board, unsigned int & nodes) {
		int losses = 0;
		Outcome outcome = Outcome::UNKNOWN;
		Side turn = board.to_play();
		Side op = ~turn;
		for (auto move : board) {
			++nodes;
			Outcome won = board.test_outcome(move, turn);

			if(won == +turn)
				return won;
			if(won == Outcome::DRAW)
				outcome = Outcome::DRAW;

			if(board.test_outcome(move, op) == +op)
				losses++;
		}
		if(losses >= 2)
			return (Outcome)op;
		return outcome;
	}
};

}; // namespace Y
}; // namespace Morat
