
#include <cmath>
#include <string>

#include "../lib/alarm.h"
#include "../lib/fileio.h"
#include "../lib/string.h"
#include "../lib/time.h"

#include "agentmcts.h"
#include "board.h"


namespace Morat {
namespace Y {

const float AgentMCTS::min_rave = 0.1;

std::string AgentMCTS::Node::to_s() const {
	return "AgentMCTS::Node"
	       ", move " + move.to_s() +
	       ", exp " + exp.to_s() +
	       ", rave " + rave.to_s() +
	       ", know " + to_str(know) +
	       ", outcome " + to_str((int)outcome.to_i()) +
	       ", depth " + to_str((int)proofdepth) +
	       ", best " + bestmove.to_s() +
	       ", children " + to_str(children.num());
}

bool AgentMCTS::Node::from_s(std::string s) {
	auto dict = parse_dict(s, ", ", " ");

	if(dict.size() == 9){
		move = Move(dict["move"]);
		exp = ExpPair(dict["exp"]);
		rave = ExpPair(dict["rave"]);
		know = from_str<int>(dict["know"]);
		outcome = Outcome(from_str<int>(dict["outcome"]));
		proofdepth = from_str<int>(dict["depth"]);
		bestmove = Move(dict["best"]);
		// ignore children
		return true;
	}
	return false;
}

void AgentMCTS::search(double time, uint64_t max_runs, int verbose){
	Side to_play = rootboard.to_play();

	if(rootboard.outcome() >= Outcome::DRAW || (time <= 0 && max_runs == 0)){
                root_outcome = rootboard.outcome().to_s_rel(to_play);
		return;
	}

	Time starttime;

	pool.pause();

	if(runs)
		logerr("Pondered " + to_str(runs) + " runs\n");

	runs = 0;
	maxruns = max_runs;
	pool.reset();

	//let them run!
	pool.resume();

	pool.wait_pause(time);

	double time_used = Time() - starttime;


	if(verbose){
		DepthStats gamelen, treelen;
		DepthStats win_types[2][Board::num_win_types];
		uint64_t games = 0, draws = 0;
		double times[4] = {0,0,0,0};
		for(auto & t : pool){
			gamelen += t->gamelen;
			treelen += t->treelen;

			for(int a = 0; a < 2; a++){
				for(int b = 0; b < Board::num_win_types; b++){
					win_types[a][b] += t->win_types[a][b];
					games += t->win_types[a][b].num;
				}
			}

			for(int a = 0; a < 4; a++)
				times[a] += t->times[a];
		}
		draws = gamelen.num - games;

		logerr("Finished:    " + to_str(runs) + " runs in " + to_str(time_used*1000, 0) + " msec: " + to_str(runs/time_used, 0) + " Games/s\n");
		if(gamelen.num > 0){
			logerr("Game length: " + gamelen.to_s() + "\n");
			logerr("Tree depth:  " + treelen.to_s() + "\n");
			if(profile)
				logerr("Times:       " + to_str(times[0], 3) + ", " + to_str(times[1], 3) + ", " + to_str(times[2], 3) + ", " + to_str(times[3], 3) + "\n");

			if (Board::num_win_types > 1 || verbose >= 2) {
				logerr("Win Types:   ");
				if (draws > 0)
					logerr("Draws: " + to_str(draws*100.0/gamelen.num, 0) + "%; ");
				for (int a = 0; a < 2; a++) {
					logerr((a == 0 ? Side::P1 : Side::P2).to_s_short() + ": ");
					for (int b = 0; b < Board::num_win_types; b++) {
						if (b != 0) logerr(", ");
						logerr(Board::win_names[b]);
						logerr(" " + to_str(win_types[a][b].num*100.0/gamelen.num, 0) + "%");
					}
					logerr((a == 0 ? "; " : "\n"));
				}

				if(verbose >= 2){
					for (int a = 0; a < 2; a++) {
						for (int b = 0; b < Board::num_win_types; b++) {
							logerr("  " + (a == 0 ? Side::P1 : Side::P2).to_s_short() + " ");
							logerr(Board::win_names[b]);
							logerr(": " + win_types[a][b].to_s() + "\n");
						}
					}
				}
			}
		}

		if(root.outcome != Outcome::UNKNOWN){
			logerr("Solved as a " + root.outcome.to_s_rel(to_play) + "\n");
		}

		std::string pvstr;
		for(const auto& m : Agent::get_pv())
			pvstr += " " + m.to_s();
		logerr("PV:         " + pvstr + "\n");

		if(verbose >= 3 && !root.children.empty())
			logerr("Move stats:\n" + move_stats(vecmove()));
	}

        root_outcome = root.outcome.to_s_rel(to_play);
	pool.reset();
	runs = 0;


	if(ponder && root.outcome < Outcome::DRAW)
		pool.resume();
}

AgentMCTS::AgentMCTS(const Board & b) : Agent(b), pool(this) {
	nodes = 0;
	runs = 0;
	gclimit = 5;

	profile     = false;
	ponder      = false;
//#ifdef SINGLE_THREAD ... make sure only 1 thread
	numthreads  = 1;
	pool.set_num_threads(numthreads);
	maxmem      = 1000*1024*1024;

	msrave      = -2;
	msexplore   = 0;

	explore     = 0;
	parentexplore = false;
	ravefactor  = 500;
	decrrave    = 0;
	knowledge   = true;
	userave     = 1;
	useexplore  = 1;
	fpurgency   = 1;
	rollouts    = 5;
	dynwiden    = 0;
	logdynwiden = (dynwiden ? std::log(dynwiden) : 0);

	shortrave   = false;
	keeptree    = true;
	minimax     = 2;
	visitexpand = 1;
	gcsolved    = 100000;
	longestloss = false;

	localreply  = 5;
	locality    = 5;
	connect     = 20;
	size        = 0;
	bridge      = 100;
	dists       = 0;

	weightedrandom = false;
	rolloutpattern = true;
	lastgoodreply  = false;
	instantwin     = 0;

	for(int i = 0; i < 4096; i++)
		gammas[i] = 1;
}
AgentMCTS::~AgentMCTS(){
	pool.pause();
	pool.set_num_threads(0);

	root.dealloc(ctmem);
	ctmem.compact();
}

void AgentMCTS::set_ponder(bool p){
	if(ponder != p){
		ponder = p;
		pool.pause();

		if(ponder)
			pool.resume();
	}
}

void AgentMCTS::set_board(const Board & board, bool clear){
	pool.pause();

	nodes -= root.dealloc(ctmem);
	root = Node();
	root.exp.addwins(visitexpand+1);

	rootboard = board;

	if(ponder)
		pool.resume();
}
void AgentMCTS::move(const Move & m){
	pool.pause();

	uword nodesbefore = nodes;

	if(keeptree && root.children.num() > 0){
		Node child;

		for(Node * i = root.children.begin(); i != root.children.end(); i++){
			if(i->move == m){
				child = *i;          //copy the child experience to temp
				child.swap_tree(*i); //move the child tree to temp
				break;
			}
		}

		nodes -= root.dealloc(ctmem);
		root = child;
		root.swap_tree(child);

		if(nodesbefore > 0)
			logerr("Nodes before: " + to_str(nodesbefore) + ", after: " + to_str(nodes) + ", saved " +  to_str(100.0*nodes/nodesbefore, 1) + "% of the tree\n");
	}else{
		nodes -= root.dealloc(ctmem);
		root = Node();
		root.move = m;
	}
	assert(nodes == root.size());

	rootboard.move(m);

	root.exp.addwins(visitexpand+1); //+1 to compensate for the virtual loss
	if(rootboard.outcome() < Outcome::DRAW)
		root.outcome = Outcome::UNKNOWN;

	if(ponder)
		pool.resume();
}

double AgentMCTS::gamelen() const {
	DepthStats len;
	for(auto & t : pool)
		len += t->gamelen;
	return len.avg();
}

std::vector<Move> AgentMCTS::get_pv(const vecmove& moves) const {
	vecmove pv;

	const Node * n = & root;
	Side turn = rootboard.to_play();
	unsigned int i = 0;
	while(n && !n->children.empty()){
		Move m = (i < moves.size() ? moves[i++] : return_move(n, turn));
		pv.push_back(m);
		n = find_child(n, m);
		turn = ~turn;
	}

	if(pv.size() == 0)
		pv.push_back(Move(M_RESIGN));

	return pv;
}

std::string AgentMCTS::move_stats(const vecmove& moves) const {
	std::string s;
	const Node * node = & root;

	s += "root:\n";
	s += node->to_s() + "\n";

	if(moves.size()){
		s += "path:\n";
		for(const auto& m : moves){
			if(node){
				node = find_child(node, m);
				s += node->to_s() + "\n";
			}
		}
	}

	if(node){
		s += "children:\n";
		for(auto & n : node->children)
			s += n.to_s() + "\n";
	}
	return s;
}

Move AgentMCTS::return_move(const Node * node, Side to_play, int verbose) const {
	if(node->outcome >= Outcome::DRAW)
		return node->bestmove;

	assert(!node->children.empty());

	double val, maxval = -1000000000000.0; //1 trillion

	const Node * ret = NULL;
	for(const auto& child : node->children) {
		if(child.outcome >= Outcome::DRAW){
			if(child.outcome == to_play)             val =  800000000000.0 - child.exp.num(); //shortest win
			else if(child.outcome == Outcome::DRAW) val = -400000000000.0 + child.exp.num(); //longest tie
			else                                    val = -800000000000.0 + child.exp.num(); //longest loss
		}else{ //not proven
			if(msrave == -1) //num simulations
				val = child.exp.num();
			else if(msrave == -2) //num wins
				val = child.exp.sum();
			else
				val = child.value(msrave, 0, 0) - msexplore*sqrt(log(node->exp.num())/(child.exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			ret = &child;
		}
	}

	assert(ret);

	if(verbose)
		logerr("Score:       " + to_str(ret->exp.avg()*100., 2) + "% / " + to_str(ret->exp.num()) + "\n");

	return ret->move;
}

void AgentMCTS::garbage_collect(Node& node, Side to_play){
	for (auto& child : node.children) {
		if (child.children.num() == 0)
			continue;

		if ((node.outcome.solved() &&       // parent is solved
		     child.exp.num() > gcsolved &&  // keep the heavy nodes
		     (node.outcome != to_play ||    // loss or draw, keep the everything
		      child.outcome == to_play)) || // found the win, keep the proof tree
		    (!node.outcome.solved() &&      // parent isn't solved,
		     child.exp.num() > (child.outcome.solved() ?
		      gcsolved :                    // only keep the heavy proof tree
		      gclimit)) ){                  // but the light area still being worked on
			garbage_collect(child, ~to_play);
		} else {
			nodes -= child.dealloc(ctmem);
		}
	}
}

AgentMCTS::Node * AgentMCTS::find_child(const Node * node, const Move & move) const {
	for(auto & c : node->children)
		if(c.move == move)
			return &c;
	return NULL;
}

void AgentMCTS::gen_sgf(SGFPrinter<Move> & sgf, unsigned int limit, const Node & node, Side side) const {
	for(auto & child : node.children){
		if(child.exp.num() >= limit && (side != node.outcome || child.outcome == node.outcome)){
			sgf.child_start();
			sgf.move(side, child.move);
			sgf.comment(child.to_s());
			gen_sgf(sgf, limit, child, ~side);
			sgf.child_end();
		}
	}
}

void AgentMCTS::create_children_simple(const Board & board, Node * node){
	assert(node->children.empty());

	node->children.alloc(board.moves_avail(), ctmem);

	Node * child = node->children.begin();
	for (auto move : board) {
		*child++ = Node(move);
	}
	assert(child == node->children.end());
	PLUS(nodes, node->children.num());
}

void AgentMCTS::load_sgf(SGFParser<Move> & sgf, const Board & board, Node & node) {
	assert(sgf.has_children());
	create_children_simple(board, & node);

	while(sgf.next_child()){
		Move m = sgf.move();
		Node & child = *find_child(&node, m);
		child.from_s(sgf.comment());
		if(sgf.done_child()){
			continue;
		}else{
			// has children!
			Board b = board;
			b.move(m);
			load_sgf(sgf, b, child);
			assert(sgf.done_child());
		}
	}
}

}; // namespace Y
}; // namespace Morat
