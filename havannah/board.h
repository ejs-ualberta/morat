
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "../lib/bitcount.h"
#include "../lib/board_grid_hex.h"
#include "../lib/board_shape_hex.h"
#include "../lib/hashset.h"
#include "../lib/move.h"
#include "../lib/outcome.h"
#include "../lib/string.h"
#include "../lib/types.h"
#include "../lib/zobrist.h"


namespace Morat {
namespace Havannah {

class Board : public BoardGridHex<Board>, public BoardShapeHex<Board> {
public:

	static constexpr const char * name = "havannah";
	static constexpr const char * default_size = "8";
	static const int min_size = 3;
	static const int max_size = 10;
	static const int max_vec_size = 19 * 19;

	static const int num_win_types = 3;
	static const char* win_names[num_win_types];

	static const int unique_depth = 5; //update and test rotations/symmetry with less than this many pieces on the board
	static const int LBDist_directions = 12;

	static const int pattern_cells = 18;

	struct Cell {
		Side    piece;   //who controls this cell, 0 for none, 1,2 for players
		uint8_t size;    //size of this group of cells
mutable uint16_t parent; //parent for this group of cells
		uint8_t corner;  //which corners are this group connected to
		uint8_t edge;    //which edges are this group connected to
mutable uint8_t mark;    //when doing a ring search, has this position been seen?
		uint8_t perm;    //is this a permanent piece or a randomly placed piece?
		Pattern pattern; //the pattern of pieces for neighbors, but from their perspective. Rotate 180 for my perpective

		Cell() : piece(Side::NONE), size(0), parent(0), corner(0), edge(0), mark(0), perm(0), pattern(0) { }
		Cell(Side p, unsigned int a, unsigned int s, unsigned int c, unsigned int e, Pattern t) :
			piece(p), size(s), parent(a), corner(c), edge(e), mark(0), perm(0), pattern(t) { }

		int numcorners() const { return BitsSetTable256[corner]; }
		int numedges()   const { return BitsSetTable256[edge];   }

		std::string to_s(int i) const;
	};

private:
	int8_t size_;  // the diameter of the board
	int8_t size_r_;  // the radius of the board
	int8_t size_r_m1_;  // size_r_ - 1

	short num_cells_;
	short num_moves_;
	Move last_move_;
	Side to_play_;
	Outcome outcome_;
	int8_t win_type_;

	std::vector<Cell> cells_;
	Zobrist<12> hash;
	std::shared_ptr<MoveValid> neighbor_list_;

public:
	bool check_rings; // whether to look for rings at all
	int perm_rings;   // how many permanent stones are needed for a ring to count

	Board() = delete;
	explicit Board(std::string s) {
		assert(size(s));
	}

	bool size(std::string s) {
		if (!valid_size(s))
			return false;
		size_r_ = from_str<int>(s);
		size_r_m1_ = size_r_ - 1;
		size_ = size_r_ * 2 - 1;
		neighbor_list_ = gen_neighbor_list();
		num_cells_ = vec_size() - size_r_ * size_r_m1_;
		cells_.resize(vec_size());
		clear();
		return true;
	}

	void clear() {
		last_move_ = M_NONE;
		num_moves_ = 0;
		to_play_ = Side::P1;
		outcome_ = Outcome::UNKNOWN;
		win_type_ = -1;
		check_rings = true;
		perm_rings = 0;
		hash.clear();

		for(int y = 0; y < size_; y++){
			for(int x = 0; x < size_; x++){
				MoveValid pos = move_valid(x, y);
				Side s = (on_board(x, y) ? Side::NONE : Side::UNDEF);
				cells_[pos.xy] = Cell(s, pos.xy, 1, (1 << iscorner(x, y)), (1 << isedge(x, y)), init_pattern(pos));
			}
		}
	}

	std::string size() const {
		return to_str<int>(size_r_);
	}

	static bool valid_size(const std::string& s) {
		int size = from_str<int>(s);
		return (min_size <= size && size <= max_size);
	}

	int mem_size() const { return sizeof(Board) + sizeof(Cell)*vec_size(); }
	int vec_size() const { return size_*size_; }
	int num_cells() const { return num_cells_; }

	int moves_made() const { return num_moves_; }
	int moves_avail() const { return (outcome() >= Outcome::DRAW ? 0 : num_cells_ - num_moves_); }
	int moves_remain() const { return moves_avail(); }

	int xy(int x, int y)   const { return   y*size_ +   x; }
	int xy(const Move & m) const { return m.y*size_ + m.x; }
	int xy(const MoveValid & m) const { return m.xy; }

	int xyc(int x, int y)   const { return xy(  x + size_r_m1_,   y + size_r_m1_); }
	int xyc(const Move & m) const { return xy(m.x + size_r_m1_, m.y + size_r_m1_); }

	MoveValid yx(int i) const { return MoveValid(i % size_, i / size_, i); }

	int dist(const Move & a, const Move & b) const {
		return (abs(a.x - b.x) + abs(a.y - b.y) + abs((a.x - a.y) - (b.x - b.y)) )/2;
	}

	const Cell * cell(int i)          const { return & cells_[i]; }
	const Cell * cell(int x, int y)   const { return cell(xy(x,y)); }
	const Cell * cell(const Move & m) const { return cell(xy(m)); }
	const Cell * cell(const MoveValid & m) const { return cell(m.xy); }

	//assumes valid x,y
	Side get(int i)          const { return cells_[i].piece; }
	Side get(int x, int y)   const { return get(xy(x, y)); }
	Side get(const Move & m) const { return get(xy(m)); }
	Side get(const MoveValid & m) const { return get(m.xy); }

	//assumes x, y are in bounds and the game isn't already finished
	bool valid_move_fast(int i)               const { return get(i) == Side::NONE; }
	bool valid_move_fast(int x, int y)        const { return valid_move_fast(xy(x, y)); }
	bool valid_move_fast(const Move & m)      const { return valid_move_fast(xy(m)); }
	bool valid_move_fast(const MoveValid & m) const { return valid_move_fast(m.xy); }
	//checks array bounds too
	bool valid_move(int x, int y)        const { return (outcome_ < Outcome::DRAW && on_board(x, y) && valid_move_fast(x, y)); }
	bool valid_move(const Move & m)      const { return (outcome_ < Outcome::DRAW && on_board(m)    && valid_move_fast(m)); }
	bool valid_move(const MoveValid & m) const { return (outcome_ < Outcome::DRAW && m.on_board()   && valid_move_fast(m)); }

	std::string to_s(bool color) const;
	std::string to_s(bool color, std::function<std::string(Move)> func) const;

	friend std::ostream& operator<< (std::ostream &out, const Board & b) { return out << b.to_s(true); }

	void print(bool color = true) const {
		printf("%s", to_s(color).c_str());
	}

	Outcome outcome() const {
		return outcome_;
	}

	int8_t win_type() const { return win_type_; }

	Side to_play() const {
		return to_play_;
	}

	Cell test_cell(const Move & pos) const {
		Side turn = to_play();
		int posxy = xy(pos);

		Cell testcell = cells_[find_group(posxy)];
		auto it = neighbors_small(posxy);
		for (const MoveValid *i = it.begin(), *e = it.end(); i < e; i++) {
			if(i->on_board() && turn == get(i->xy)){
				const Cell * g = & cells_[find_group(i->xy)];
				testcell.corner |= g->corner;
				testcell.edge   |= g->edge;
				testcell.size   += g->size; //not quite accurate if it's joining the same group twice
				i++; //skip the next one
			}
		}
		return testcell;
	}

	int test_connectivity(const Move & pos) const {
		Cell testcell = test_cell(pos);
		return testcell.numcorners() + testcell.numedges();
	}

	int test_size(const Move & pos) const {
		Cell testcell = test_cell(pos);
		return testcell.size;
	}

	//check if a position is encirclable by a given player
	//false if it or one of its neighbors are the opponent's and connected to an edge or corner
	bool encirclable(const Move pos, Side player) const {
		Side otherplayer = ~player;
		int posxy = xy(pos);

		const Cell * g = & cells_[find_group(posxy)];
		if(g->piece == otherplayer && (g->edge || g->corner))
			return false;

		for (auto m : neighbors_small(posxy)) {
			if(!m.on_board())
				return false;

			const Cell * g = cell(find_group(m.xy));
			if(g->piece == otherplayer && (g->edge || g->corner))
				return false;
		}
		return true;
	}

	hash_t gethash() const {
		return (num_moves_ > unique_depth ? hash.get(0) : hash.get());
	}

	void update_hash(const MoveValid & pos, Side side) {
		int turn = side.to_i();
		if(num_moves_ > unique_depth){ //simple update, no rotations/symmetry
			hash.update(0, 3 * pos.xy + turn);
			return;
		}

		//mirror is simply flip x,y
		int x = pos.x - size_r_m1_,
		    y = pos.y - size_r_m1_,
		    z = y - x;

//x,y; y,z; z,-x; -x,-y; -y,-z; -z,x
//y,x; z,y; -x,z; -y,-x; -z,-y; x,-z

		hash.update(0,  3*xyc( x,  y) + turn);
		hash.update(1,  3*xyc( y,  z) + turn);
		hash.update(2,  3*xyc( z, -x) + turn);
		hash.update(3,  3*xyc(-x, -y) + turn);
		hash.update(4,  3*xyc(-y, -z) + turn);
		hash.update(5,  3*xyc(-z,  x) + turn);
		hash.update(6,  3*xyc( y,  x) + turn);
		hash.update(7,  3*xyc( z,  y) + turn);
		hash.update(8,  3*xyc(-x,  z) + turn);
		hash.update(9,  3*xyc(-y, -x) + turn);
		hash.update(10, 3*xyc(-z, -y) + turn);
		hash.update(11, 3*xyc( x, -z) + turn);
	}

	hash_t test_hash(const MoveValid & pos) const {
		return test_hash(pos, to_play());
	}

	hash_t test_hash(const MoveValid & pos, Side side) const {
		int turn = side.to_i();
		if(num_moves_ >= unique_depth) //simple test, no rotations/symmetry
			return hash.test(0, 3*xy(pos) + turn);

		int x = pos.x - size_r_m1_,
		    y = pos.y - size_r_m1_,
		    z = y - x;

		hash_t m = hash.test(0,  3*xyc( x,  y) + turn);
		m = std::min(m, hash.test(1,  3*xyc( y,  z) + turn));
		m = std::min(m, hash.test(2,  3*xyc( z, -x) + turn));
		m = std::min(m, hash.test(3,  3*xyc(-x, -y) + turn));
		m = std::min(m, hash.test(4,  3*xyc(-y, -z) + turn));
		m = std::min(m, hash.test(5,  3*xyc(-z,  x) + turn));
		m = std::min(m, hash.test(6,  3*xyc( y,  x) + turn));
		m = std::min(m, hash.test(7,  3*xyc( z,  y) + turn));
		m = std::min(m, hash.test(8,  3*xyc(-x,  z) + turn));
		m = std::min(m, hash.test(9,  3*xyc(-y, -x) + turn));
		m = std::min(m, hash.test(10, 3*xyc(-z, -y) + turn));
		m = std::min(m, hash.test(11, 3*xyc( x, -z) + turn));
		return m;
	}

	bool move(const Move & pos, bool checkwin = true, bool permanent = true) {
		return move(MoveValid(pos, xy(pos)), checkwin, permanent);
	}
	bool move(const MoveValid & pos, bool checkwin = true, bool permanent = true) {
		assert(!outcome_.solved());

		if(!valid_move(pos))
			return false;

		last_move_ = pos;
		num_moves_++;

		Cell& cell = cells_[pos.xy];
		cell.piece = to_play_;
		cell.perm = permanent;

		update_hash(pos, to_play_); //depends on num_moves_
		update_pattern(pos, to_play_);

		// join the groups for win detection
		bool alreadyjoined = false; //useful for finding rings
		auto it = neighbors_small(pos);
		for (const MoveValid *i = it.begin(), *e = it.end(); i < e; i++) {
			if(i->on_board() && to_play_ == get(i->xy)){
				alreadyjoined |= join_groups(pos.xy, i->xy);
				i++; //skip the next one. If it is the same group,
					 //it is already connected and forms a corner, which we can ignore
			}
		}

		if(checkwin){
			Cell * g = & cells_[find_group(pos.xy)];
			if(g->numedges() >= 3){
				outcome_ = +to_play_;
				win_type_ = 0;
			}else if(g->numcorners() >= 2){
				outcome_ = +to_play_;
				win_type_ = 1;
			}else if(check_rings && alreadyjoined && g->size >= 6 && checkring_df(pos, to_play_)){
				outcome_ = +to_play_;
				win_type_ = 2;
			}else if(num_moves_ == num_cells_){
				outcome_ = Outcome::DRAW;
			}
		}

		to_play_ = ~to_play_;

		return true;
	}

	//test if making this move would win, but don't actually make the move
	Outcome test_outcome(const Move & pos) const { return test_outcome(pos, to_play()); }
	Outcome test_outcome(const Move & pos, Side turn) const { return test_outcome(MoveValid(pos, xy(pos)), turn); }
	Outcome test_outcome(const MoveValid & pos) const { return test_outcome(pos, to_play()); }
	Outcome test_outcome(const MoveValid & pos, Side turn) const {
		if(test_local(pos, turn)){
			Cell testcell = cells_[find_group(pos.xy)];
			int numgroups = 0;
			auto it = neighbors_small(pos);
			for (const MoveValid *i = it.begin(), *e = it.end(); i < e; i++) {
				if(i->on_board() && turn == get(i->xy)){
					const Cell * g = & cells_[find_group(i->xy)];
					testcell.corner |= g->corner;
					testcell.edge   |= g->edge;
					testcell.size   += g->size;
					i++; //skip the next one
					numgroups++;
				}
			}

			if(testcell.numcorners() >= 2 || testcell.numedges() >= 3 || (check_rings && numgroups >= 2 && testcell.size >= 6 && checkring_o1(pos, turn)))
				return +turn;
		}

		if(num_moves_+1 == num_cells_)
			return Outcome::DRAW;

		return Outcome::UNKNOWN;
	}

private:
	int iscorner(int x, int y) const;
	int isedge(int x, int y) const;

	bool checkring_df(const MoveValid & pos, const Side turn) const;
	bool checkring_o1(const MoveValid & pos, const Side turn) const;
	bool followring(const MoveValid & cur, const int & dir, const Side & turn, const int & permsneeded) const;
	bool checkring_back(const MoveValid & a, const MoveValid & b, const MoveValid & c, Side turn) const;

	std::shared_ptr<MoveValid> gen_neighbor_list() const;

	int find_group(const MoveValid & m) const { return find_group(m.xy); }
	int find_group(unsigned int i) const {
		unsigned int p = cells_[i].parent;
		if(p != i){
			do{
				p = cells_[p].parent;
			}while(p != cells_[p].parent);
			cells_[i].parent = p; //do path compression, but only the current one, not all, to avoid recursion
		}
		return p;
	}

	//join the groups of two positions, propagating group size, and edge/corner connections
	//returns true if they're already the same group, false if they are now joined
	bool join_groups(int i, int j){
		i = find_group(i);
		j = find_group(j);

		if(i == j)
			return true;

		if(cells_[i].size < cells_[j].size) //force i's subtree to be bigger
			std::swap(i, j);

		cells_[j].parent = i;
		cells_[i].size   += cells_[j].size;
		cells_[i].corner |= cells_[j].corner;
		cells_[i].edge   |= cells_[j].edge;

		return false;
	}

	friend class BoardGridHex;
	friend class BoardShapeHex;
	friend class BoardBase;
};

}; // namespace Havannah
}; // namespace Morat
