/*
  Solvitaire: a solver for perfect information solitaire games
  Copyright (C) 2018 Charles Blake <thecharlesblake@live.co.uk> and 
  Ian Gent <Ian.Gent@st-andrews.ac.uk>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program (see LICENSE file); if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
//
// Created by thecharlesblake on 10/24/17.
//

#include <string>
#include <ostream>
#include <iostream>
#include <algorithm>
#include <list>
#include <malloc.h>
#include <chrono>
#include <iomanip>
#include <signal.h>

#include "solver.h"
#include "../game/move.h"
#include "../input-output/output/log_helper.h"
#include "../input-output/output/state_printer.h"
#include "../input-output/input/command_line_helper.h"

using std::atomic;
using std::vector;
using std::cout;
using std::clog;
using std::pair;
using std::max;
using std::begin;
using std::end;
using boost::optional;
using std::fixed;
using std::setprecision;

static bool sigint = false;

void sigint_handler(int i) {
    // So it doesn't complain about unused param
    sigint = i == 1 ? true : true;
}

solver::solver(const game_state& gs, uint64_t cache_capacity)
        : cache(lru_cache(gs, cache_capacity))
        , init_state(gs)
        , state(gs)
        , frontier()
        , root(move(move::mtype::null))
        , current_node() {
    frontier.push_back(root);
    current_node = begin(frontier);
    res.states_searched = 0;
    res.unique_states_searched = 0;
    res.backtracks = 0;
    res.dominance_moves = 0;
    res.states_removed_from_cache = 0;
    res.max_depth = 0;
    res.depth = 0;
}

solver::node::node(const move m) noexcept
        : mv(m), child_moves(), cache_state() {
}

//  void solver::operator= (const solver &slv) {
//      result = slv.result;
// 	sol_type = slv.sol_type;
//     states_searched = slv.states_searched;
//     unique_states_searched = slv.unique_states_searched;
//     backtracks = slv.backtracks;
//     dominance_moves = slv.dominance_moves;
//     states_removed_from_cache = slv.states_removed_from_cache;
//     cache_size = slv.cache_size;
//     cache_bucket_count = slv.cache_bucket_count;
//     depth = slv.depth;
//     time = slv.time;	
//  }

solver::result solver::run(boost::optional<millisec> timeout) {
    // Set interrupt handler
    signal(SIGINT, sigint_handler);

    // Set timings
    const clock::time_point start_time = clock::now();
    result::type res_type = timeout ? dfs(start_time + *timeout) : dfs();
    res.sol_type = res_type;
    res.states_removed_from_cache = cache.get_states_removed_from_cache();
    res.cache_size = cache.size();
    res.cache_bucket_count = cache.bucket_count();
    res.time = std::chrono::duration_cast<millisec>(clock::now() - start_time);

    return res;
}


solver::result solver::run_DLS(uint64_t depth_limit, boost::optional<millisec> timeout) {
    // Set interrupt handler
    signal(SIGINT, sigint_handler);

    // Set timings
    const clock::time_point start_time = clock::now();

    result dls_reult = timeout ? dls(depth_limit, start_time + *timeout) : dls(depth_limit);
    res.sol_type = dls_reult.sol_type;
    res.states_removed_from_cache = cache.get_states_removed_from_cache();
    res.cache_size = cache.size();
    res.cache_bucket_count = cache.bucket_count();
    res.time = std::chrono::duration_cast<millisec>(clock::now() - start_time);
   
    return res;
}

solver::result::type solver::dfs(boost::optional<clock::time_point> end_time) {
    bool states_exhausted = false;

    while(!(state.is_solved() || states_exhausted)) {
        if (end_time && clock::now() >= *end_time) {
            return result::type::TIMEOUT;
        } else if (sigint) {
            return result::type::TERMINATED;
        }

#ifndef NDEBUG
        if (current_node->mv.dominance_move) {
            LOG_DEBUG("(dominance move)");
        }
        LOG_DEBUG(state);
#endif

        // If there is a dominance move available, adds it to the search tree
        // and repeats. Doesn't cache the state.
        optional<move> dominance_move = state.get_dominance_move();
        if (dominance_move) {
            // Adds the dominance move as a child of the current search node;
            current_node->child_moves.emplace_back(*dominance_move);
        } else {
            try {
                // Caches the current state
                pair<lru_cache::item_list::iterator, bool> insert_res = cache.insert(state);
                current_node->cache_state = insert_res.first;
                bool is_new_state = insert_res.second;

                if (is_new_state) {
                    // Gets the legal moves in the current state
                    vector<move> next_moves = state.get_legal_moves(current_node->mv);

                    // If there are none, reverts to the last node with children
                    if (next_moves.empty()) {
                        states_exhausted = revert_to_last_node_with_children(insert_res.first);
                    } else {
                        current_node->child_moves = std::move(next_moves);
                    }
                }
                    // If the state is not a new one, reverts to the last node with children
                else {
                    res.unique_states_searched--;
                    states_exhausted = revert_to_last_node_with_children();
                }
            } catch (const std::runtime_error &e) {
                return result::type::MEM_LIMIT;
            }
        }

        // Sets the current node to one of its children
        assert(states_exhausted == current_node->child_moves.empty());
        if (!states_exhausted) {
            set_to_child();
            state.make_move(current_node->mv);
            res.depth++;
            res.max_depth = max(res.depth, res.max_depth);
            if (current_node->mv.dominance_move) res.dominance_moves++;
        }

        res.states_searched++;
        res.unique_states_searched++;
    }

    if (state.is_solved()) {
        return result::type::SOLVED;
    } else {
        assert(states_exhausted);
        return result::type::UNSOLVABLE;
    }
}

// Called when the current node's children have been exhausted. Travels back up
// the search tree until it finds a node which still has children. Returns true
// unless all children have been exhausted.

// If an iterator to the current state is supplied to the function, will also
// make sure to turn the 'live' bit off upon backtracking
bool solver::revert_to_last_node_with_children(optional<lru_cache::item_list::iterator> cur_state) {
    if (current_node == begin(frontier))
        return true;

    // Turns the 'live' bit false on the state we are backtracking out of
    if (cur_state) cache.set_non_live(*cur_state);

    state.undo_move(current_node->mv);
    res.depth--;
    res.backtracks++;

#ifndef NDEBUG
    // Checks that the state after the undo is in the cache
    // (as long as the move wasn't a dominance move)

    if (! current_node->mv.dominance_move) {
        if (cache.get_states_removed_from_cache() == 0) {
            assert(cache.contains(state));
        }
        LOG_DEBUG("(undo move)");
    } else {
        LOG_DEBUG("(undo dominance move)");
    }
    LOG_DEBUG(state);
#endif

    // Gets a reference to the parent state which can be supplied if this function is
    // called recursively. This ensures that the cached state's 'live' bit is set as appropriate
    optional<lru_cache::item_list::iterator> p_state = prev(current_node)->cache_state;

    // Reverts the current node to its parent and removes it
    frontier.pop_back();
    current_node = prev(end(frontier));

    // If the current node now has no children, repeat
    if (current_node->child_moves.empty()) {
        return revert_to_last_node_with_children(p_state);
    } else {
        return false;
    }
}

void solver::set_to_child() {
    assert(!current_node->child_moves.empty());

    move b = current_node->child_moves.back();
    current_node->child_moves.pop_back();
    frontier.emplace_back(b);

    current_node = prev(end(frontier));
}

void solver::print_solution() const {
    std::flush(clog);
    std::flush(cout);

    auto i = begin(frontier);
    game_state state_copy = init_state;

    cout << "Solution:\n";
    cout << state_copy << "\n";

    if (res.states_searched > 1) {
        while (++i != end(frontier)) {
            state_copy.make_move(i->mv);
            cout << state_copy << "\n";
        }
    }
    cout << "\n";
}

// std::ostream& operator<< (std::ostream& out, const solver::result::type& rt) {v);
//             cout << state_copy << "\n";
//         }
//     }
//     cout << "\n";
// }

std::ostream& operator<< (std::ostream& out, const solver::result::type& rt) {
    switch(rt) {
        case solver::result::type::TIMEOUT:
            out << "timed-out";
            break;
        case solver::result::type::SOLVED:
            out << "solved";
            break;
        case solver::result::type::UNSOLVABLE:
            out << "unsolvable";
            break;
        case solver::result::type::MEM_LIMIT:
            out << "memory-limit-reached";
            break;
        case solver::result::type::TERMINATED:
            out << "terminated";
            break;
    }
    return out;
}

std::ostream& operator<< (std::ostream& out, const solver::result& r) {
    return out
            << "Solution Type: "             << r.sol_type                   << "\n"
            << "States Searched: "           << r.states_searched            << "\n"
            << "Unique States Searched: "    << r.unique_states_searched     << "\n"
            << "Backtracks: "                << r.backtracks                 << "\n"
            << "Dominance Moves: "           << r.dominance_moves            << "\n"
            << "States Removed From Cache: " << r.states_removed_from_cache  << "\n"
            << "Final States In Cache: "     << r.cache_size                 << "\n"
            << "Final Buckets In Cache: "    << r.cache_bucket_count         << "\n"
            << "Maximum Search Depth: "      << r.max_depth                  << "\n"
            << "Final Search Depth: "        << r.depth                      << "\n"
            << "Time Taken (milliseconds): " << r.time.count()               << "\n";
}

void solver::print_header(long t, command_line_helper::streamliner_opt stream_opt) {
    cout << "Calculating solvability percentage...\n\n";
    if (stream_opt == command_line_helper::streamliner_opt::SMART) {
        cout << ", (Streamliner Results:) "
                "Attempted Seed"
                ", Outcome"
                ", Time Taken(ms)"
                ", States Searched"
                ", Unique States Searched"
                ", Backtracks"
                ", Dominance Moves"
                ", States Removed From Cache"
                ", Final States In Cache"
                ", Final Buckets In Cache"
                ", Maximum Search Depth"
                ", Final Search Depth"
                ", (Non-Streamliner Results:) ";
    }
    cout << "Attempted Seed"
            ", Outcome"
            ", Time Taken(ms)"
            ", States Searched"
            ", Unique States Searched"
            ", Backtracks"
            ", Dominance Moves"
            ", States Removed From Cache"
            ", Final States In Cache"
            ", Final Buckets In Cache"
            ", Maximum Search Depth"
            ", Final Search Depth"
            ", Overall Result"
            "\n--- Timeout = " << t << " milliseconds ---\n";
    cout << fixed << setprecision(3);
}

void solver::print_result_csv(solver::result res) {
    cout << ", " << res.sol_type
         << ", " << res.time.count()
         << ", " << res.states_searched
         << ", " << res.unique_states_searched
         << ", " << res.backtracks
         << ", " << res.dominance_moves
         << ", " << res.states_removed_from_cache
         << ", " << res.cache_size
         << ", " << res.cache_bucket_count
         << ", " << res.max_depth
         << ", " << res.depth;
}

void solver::print_null_seed_info() {
    cout << ", , , , , , , , , , , ";
}

const vector<solver::node> solver::get_frontier() const {
    return frontier;
}
