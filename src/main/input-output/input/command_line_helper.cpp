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
// Created by thecharlesblake on 10/16/17.
//

#include <tuple>
#include <vector>

#include <boost/program_options.hpp>

#include "command_line_helper.h"
#include "../../game/sol_rules.h"
#include "../output/log_helper.h"
#include "sol_preset_types.h"

using namespace std;

namespace po = boost::program_options;

command_line_helper::command_line_helper()
        : main_options("options") {

    main_options.add_options()
            ("help", "produce help message")
            ("version", "the version of Solvitaire being used")
            ("type", po::value<string>(),
             "specify the type of the solitaire game to be solved from the list "
                     "of preset games. Must supply either this 'type' option, "
                     "or the 'custom-rules' option")
            ("available-game-types", "outputs a list of the different preset "
                    "game types that can be solved")
            ("describe-game-rules", po::value<string>(),
                    "outputs the JSON that describes the rules of the supplied "
                    "preset game type")
            ("custom-rules", po::value<string>(),
             "the path to a JSON file describing the rules of the solitaire "
                     "to be solved. Must supply either 'type' or 'custom-rules' option")
            ("random", po::value<int>(), "create and solve a random solitaire "
                    "deal based on a seed. Must supply either 'random',"
                    "'solvability', 'benchmark' or list of deals to be solved.")
            ("classify", "outputs a simple 'solvable/not solvable' "
                         "classification")
            ("cache-capacity", po::value<uint64_t>(), "sets an upper bound on the number of states allowed in "
                               "the cache")
            ("solvability", po::value<int>(), "calculates the solvability "
                    "percentage of the supplied solitaire game, given a limit for the number of seeds. Must supply "
                    "either 'random', 'benchmark', 'solvability' or list of deals to be solved.")
            ("timeout", po::value<uint64_t>(), "adds a timeout to searches")
            ("resume", po::value<vector<int>>()->multitoken(), "resumes the solvability percentage calculation from a "
                                                    "previous run. Must be supplied with the solvability option. "
                                                    "Syntax: [sol unsol intract in-progress-1 in-progress-2 ...]")
            ("cores", po::value<uint>(), "the number of cores for the solvability percentages to be run across. "
                                         "Must be supplied with the solvability option.")
            ("streamliners", po::value<string>(),
                    "Applies streamliners to the search. Options include 'none', 'both', 'suit-symmetry',"
                    " 'auto-foundations', and 'smart-solvability'. Defaults to 'none', unless '--solvability' is"
                    " also supplied, in which case defaults to 'smart-solvability'. 'smart-solvability' mode"
                    " runs first with both streamliners and a 10% timeout. If this"
                    " is unsuccessful (unsolvable or timeout), then runs again without streamliners.")
            ("benchmark", "outputs performance statistics for the solver on the "
                          "supplied solitaire game. Must supply "
                          "either 'random', 'benchmark', 'solvability' or list of deals to be "
                          "solved.")
            ("deal-only", "outputs the starting deal for a given game type & random seed as json")
            ("iddfs", "if true, preform iterative-deeping-DFS, which returns an optimal* solution (minimal depth)")
            ("dls", po::value<int>(),"preform Depth Limited Search with the depth limit that was proveded")
            ("print_path", "outputs the path from init state to the winning state");

    po::options_description hidden_options("Hidden options");
    hidden_options.add_options()
            ("input-files", po::value<vector<string>>(),
             "the file containing the deal to be solved");

    cmdline_options.add(main_options).add(hidden_options);

    p.add("input-files", -1);
}

// Returns true if we can continue solving the supplied solitaire(s)
bool command_line_helper::parse(int argc, const char* argv[]) {
    po::variables_map vm;
    // Attempts to parse cmdln options. Catches basic errors
    try {
        store(po::command_line_parser(argc, argv).options(cmdline_options)
                      .positional(p).run(), vm);
    } catch(po::error& e) {
        LOG_ERROR ("Error: " << e.what());
        return false;
    }

    // Updates the variables map which the user's options are stored in
    notify(vm);

    // Converts the variables map into flags and values

    help = (vm.count("help") != 0);

    available_game_types = (vm.count("available-game-types") != 0);

    version = (vm.count("version") != 0);

    if (vm.count("describe-game-rules")) {
        describe_game_rules = vm["describe-game-rules"].as<string>();
    }

    classify = (vm.count("classify") != 0);

    deal_only = (vm.count("deal-only") != 0);

    print_path = (vm.count("print_path") != 0); // if true, print all the states from init -> end
    
    optimal_solution = (vm.count("iddfs") != 0); // if true, after the DFS solution, solve with id-DFS
    
    // if dls>0 use Depth limited search
    if (vm.count("dls")) {
        dls_depth_limit = vm["dls"].as<int>();
    } else {
        dls_depth_limit = -1;
    }

    if (vm.count("input-files")) {
        input_files = vm["input-files"].as<vector<string>>();
    }

    if (vm.count("type")) {
        solitaire_type = vm["type"].as<string>();
    }

    if (vm.count("custom-rules")) {
        rules_file = vm["custom-rules"].as<string>();
    }

    if (vm.count("random")) {
        random_deal = vm["random"].as<int>();
    } else {
        random_deal = -1;
    }

    if (vm.count("cache-capacity")) {
        cache_capacity = vm["cache-capacity"].as<uint64_t>();
    } else {
        cache_capacity = 100000000; // One hundred-million
    }

    if (vm.count("solvability")) {
        solvability = vm["solvability"].as<int>();
    } else {
        solvability = -1;
    }

    if (vm.count("timeout")) {
        timeout = vm["timeout"].as<uint64_t>();
    } else {
        timeout = 604800000; // 1 week in milliseconds
    }

    if (vm.count("cores")) {
        cores = vm["cores"].as<uint>();
    } else {
        cores = 1;
    }

    if (vm.count("resume")) {
        resume = vm["resume"].as<vector<int>>();
    } else {
        resume = {0, 0, 0};
        for (uint i = 0; i < cores; i++) resume.push_back(i);
    }

    if (vm.count("streamliners")) {
        auto& s = vm["streamliners"].as<string>();

        if (s == "auto-foundations" ) streamliners = streamliner_opt::AUTO_FOUNDATIONS;
        else if (s == "suit-symmetry") streamliners = streamliner_opt::SUIT_SYMMETRY;
        else if (s == "both") streamliners = streamliner_opt::BOTH;
        else if (s == "smart-solvability") streamliners = streamliner_opt::SMART;
        else if (s == "none") streamliners = streamliner_opt::NONE;
        else {
            print_streamliner_error(s);
            return false;
        }
    } else {
        if (solvability == -1) streamliners = streamliner_opt::NONE;
        else streamliners = streamliner_opt::SMART;
    }

    benchmark = (vm.count("benchmark") != 0);

    // Handle logic error scenarios
    return assess_errors();
}

bool command_line_helper::assess_errors() {
    if (help) {
        print_help();
        return false;
    }

    if (version) { return true; } 

    if (available_game_types || !describe_game_rules.empty()) return true;

    if (solvability > 0 && resume.size() != 3 + cores) {
        print_resume_error();
        return false;
    }

    // The user must either supply input files, a random seed, or ask for the
    // solvability percentage, or benchmark
    int opt_count = (random_deal != -1) + !input_files.empty() + (solvability > 0) + benchmark;

    if (opt_count > 1) {
        print_too_many_opts_error();
        return false;
    } else if (opt_count < 1) {
        print_no_opts_error();
        return false;
    }

    // The user must supply either a solitaire type or a rules file
    if ((solitaire_type.empty() && rules_file.empty())
            || (!solitaire_type.empty() && !rules_file.empty())) {
        print_sol_type_rules_error();
        return false;
    }

    if (!solitaire_type.empty()) {
        return assess_sol_type();
    } else {
        return true;
    }
}

// Checks if the supplied solitaire type is in the list of valid solitaires
bool command_line_helper::assess_sol_type() {
    if (sol_preset_types::is_valid_preset(solitaire_type)) {
        return true;

    } else {
        LOG_ERROR ("Error: Solitaire type is not valid: " << solitaire_type);
        print_help();
        return false;
    }
}

void command_line_helper::print_help() {
    cerr <<  "Usage: solvitaire [options] input-file1 input-file2 ...\n"
            << main_options << "\n";
}

void command_line_helper::print_no_opts_error() {
    LOG_ERROR ("Error: User must supply input file(s), the '--random' "
            "option, the 'benchmark' option, or the '--solvability' option");
    print_help();
}

void command_line_helper::print_sol_type_rules_error() {
    LOG_ERROR ("Error: User must supply either a solitaire type, or a 'rules' "
                       "file");
    print_help();
}

void command_line_helper::print_too_many_opts_error() {
    LOG_ERROR ("Error: User must supply input file(s), the '--random' option, the 'benchmark' option,, "
               "or the '--solvability' option, not multiple");
    print_help();
}

void command_line_helper::print_resume_error() {
    LOG_ERROR ("Error: Resume contains " << resume.size() << " options, and should contain " << cores + 3);
    print_help();
}

void command_line_helper::print_streamliner_error(const string& str) {
    LOG_ERROR ("Error: invalid streamliner: " + str + ".\nAvailable options are: 'none', 'both', 'suit-symmetry',"
                                                      " 'auto-foundations', and 'smart-solvability'");
}

const vector<string> command_line_helper::get_input_files() {
    return input_files;
}

const string command_line_helper::get_solitaire_type() {
    return solitaire_type;
}

const string command_line_helper::get_rules_file() {
    return rules_file;
}

int command_line_helper::get_random_deal() {
    return random_deal;
}

bool command_line_helper::get_optimal_solution() {
    return optimal_solution;
}
int command_line_helper::get_dls_depth_limit() {
    return dls_depth_limit;
}

bool command_line_helper::get_classify() {
    return classify;
}

bool command_line_helper::get_deal_only() {
    return deal_only;
}

bool command_line_helper::get_print_path() {
    return print_path;
}

uint64_t command_line_helper::get_cache_capacity() {
    return cache_capacity;
}

int command_line_helper::get_solvability() {
    return solvability;
}

uint64_t command_line_helper::get_timeout() {
    return timeout;
}

vector<int> command_line_helper::get_resume() {
    return resume;
}

uint command_line_helper::get_cores() {
    return cores;
}

bool command_line_helper::get_available_game_types() {
    return available_game_types;
}

string command_line_helper::get_describe_game_rules() {
    return describe_game_rules;
}

bool command_line_helper::get_benchmark() {
    return benchmark;
}

bool command_line_helper::get_version() {
    return version;
}

command_line_helper::streamliner_opt command_line_helper::get_streamliners() {
    return streamliners;
}

game_state::streamliner_options command_line_helper::get_streamliners_game_state() {
    return convert_streamliners(streamliners);
}

game_state::streamliner_options command_line_helper::convert_streamliners(streamliner_opt so) {
    switch (so){
        case command_line_helper::streamliner_opt::NONE: return game_state::streamliner_options::NONE;
        case command_line_helper::streamliner_opt::AUTO_FOUNDATIONS: return game_state::streamliner_options::AUTO_FOUNDATIONS;
        case command_line_helper::streamliner_opt::SUIT_SYMMETRY: return game_state::streamliner_options::SUIT_SYMMETRY;
        case command_line_helper::streamliner_opt::BOTH: return game_state::streamliner_options::BOTH;
        case command_line_helper::streamliner_opt::SMART:
        default:
            assert(false);
            throw runtime_error("attempted to convert smart streamliner mode to single streamliner");
            return game_state::streamliner_options::NONE;;
    }
}
