//
// Created by thecharlesblake on 10/16/17.
//

#include <tuple>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>

#include "command_line_helper.h"

using namespace std;

namespace po = boost::program_options;

command_line_helper::command_line_helper()
        : main_options("options") {

    main_options.add_options()
            ("help", "produce help message")
            ("type", po::value<string>(),
             "specify the type of the solitaire game to be solved (REQUIRED)")
            ("random", "create and solve a random solitaire deal");

    po::options_description hidden_options("Hidden options");
    hidden_options.add_options()
            ("input-files", po::value<vector<string>>(),
             "the file containing the deal to be solved");

    cmdline_options.add(main_options).add(hidden_options);

    p.add("input-files", -1);
}

bool command_line_helper::parse(int argc, const char* argv[]) {
    po::variables_map vm;
    try {
        store(po::command_line_parser(argc, argv).options(cmdline_options)
                      .positional(p).run(), vm);
    } catch(po::error& e) {
        cerr << "Error: " << e.what() << "\n";
        return false;
    }

    notify(vm);

    help = vm.count("help");

    if (vm.count("input-files")) {
        input_files = vm["input-files"].as<vector<string>>();
    }

    if (vm.count("type")) {
        solitaire_type = vm["type"].as<string>();
    }

    random_deal = vm.count("random");

    // Handle logic error scenarios
    return assess_errors();
}

bool command_line_helper::assess_errors() {
    if (help) {
        print_help();
        return false;
    }

    if (solitaire_type.empty()) {
        print_no_sol_type_error();
        return false;
    }

    if (random_deal && !input_files.empty()) {
        print_rand_plus_input_err();
        return false;
    }

    if (input_files.empty() && !random_deal) {
        print_no_input_error();
        return false;
    }
    return true;
}

void command_line_helper::print_help() {
    cout << "Usage: solvitaire [options] input-file1 input-file2 ...\n"
            << main_options << "\n";
}

void command_line_helper::print_no_input_error() {
    cerr << "Error: User must supply input file(s) or the '--random' "
            "option\n";
    print_help();
}

void command_line_helper::print_no_sol_type_error() {
    cerr << "Error: User must supply a solitaire type\n";
    print_help();
}

void command_line_helper::print_rand_plus_input_err() {
    cerr << "Error: User must supply input file(s) or the '--random' option, "
            "not both\n";
    print_help();
}

const vector<string> command_line_helper::get_input_files() {
    return input_files;
}

const string command_line_helper::get_solitaire_type() {
    return solitaire_type;
}

bool command_line_helper::get_random_deal() {
    return random_deal;
}

bool command_line_helper::get_help() {
    return help;
}