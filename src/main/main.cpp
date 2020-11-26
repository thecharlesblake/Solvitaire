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
#include <boost/program_options.hpp>
#include <boost/optional.hpp>

#include "version.h"
#include "../../lib/rapidjson/document.h"
#include "input-output/input/command_line_helper.h"
#include "input-output/input/sol_preset_types.h"
#include "input-output/input/json-parsing/json_helper.h"
#include "input-output/input/json-parsing/rules_parser.h"
#include "input-output/output/log_helper.h"
#include "solver/solver.h"
#include "evaluation/solvability_calc.h"
#include "evaluation/benchmark.h"

using namespace rapidjson;

using namespace std;
using namespace boost;

namespace po = boost::program_options;

typedef std::chrono::milliseconds millisec;

const optional<sol_rules> gen_rules(command_line_helper&);
void solve_random_game(int, const sol_rules&, command_line_helper&);
void solve_input_files(vector<string>, const sol_rules&, command_line_helper&);
void solve_game(const sol_rules& rules, command_line_helper& clh, optional<int> seed, optional<const Document&> in_doc);
pair<solver, solver::result> solve_game(const sol_rules& rules, uint64_t timeout, uint64_t cache_capacity,
                                        game_state::streamliner_options str_opts,
                                        optional<int> seed, optional<const Document &> in_doc, bool iddfs, int dls_depth_limit);
boost::tuple<solver, solver::result, bool> run_iddfs(uint64_t optimal_depth, const sol_rules &rules, uint64_t timeout, uint64_t cache_capacity,
                                       game_state::streamliner_options str_opts,
                                       optional<int> seed, optional<const Document &> in_doc);
void print_version();

// Decides what to do given supplied command-line options
int main(int argc, const char *argv[])
{

    // Parses the command-line options
    command_line_helper clh;
    if (!clh.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    // If the user has asked for the list of preset game types, prints it
    if (clh.get_available_game_types())
    {
        sol_preset_types::print_available_games();
        return EXIT_SUCCESS;
    }

    // If the user has asked for the version, prints it
    if (clh.get_version()) {
        print_version();
        return EXIT_SUCCESS;
    }

    string game_rule_str = clh.get_describe_game_rules();
    if (!game_rule_str.empty())
    {
        sol_preset_types::describe_game_rules(game_rule_str);
        return EXIT_SUCCESS;
    }

    // Generates the rules of the solitaire from the game type
    const optional<sol_rules> rules = gen_rules(clh);
    if (!rules)
        return EXIT_FAILURE;

    if (clh.get_deal_only())
    {
        game_state gs(*rules, clh.get_random_deal(), game_state::streamliner_options::NONE);
        json_helper::print_game_state_as_json(gs);
        return EXIT_SUCCESS;
    }

    // If the user has asked for a solvability percentage, calculates it
    if (clh.get_solvability() > 0)
    {
        solvability_calc solv_c(*rules, clh.get_cache_capacity());
        solv_c.calculate_solvability_percentage(clh.get_timeout(), clh.get_solvability(), clh.get_cores(),
                                                clh.get_streamliners(), clh.get_resume());
    }
