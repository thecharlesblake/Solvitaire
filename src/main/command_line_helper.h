//
// Created by thecharlesblake on 10/16/17.
//

#ifndef SOLVITAIRE_COMMAND_LINE_HELPER_H
#define SOLVITAIRE_COMMAND_LINE_HELPER_H

#include <boost/program_options.hpp>

class command_line_helper {
public:
    command_line_helper();
    bool parse(int argc, const char* argv[]);
    const std::vector<std::string> get_input_files();
    const std::string get_solitaire_type();
    int get_random_deal();
    bool get_help();
private:
    bool assess_errors();
    void print_help();
    void print_no_input_error();
    void print_no_sol_type_error();
    void print_rand_plus_input_err();
    
    boost::program_options::options_description cmdline_options;
    boost::program_options::options_description main_options;
    boost::program_options::positional_options_description p;

    std::vector<std::string> input_files;
    std::string solitaire_type;
    int random_deal;
    bool help;
};

#endif //SOLVITAIRE_COMMAND_LINE_HELPER_H
