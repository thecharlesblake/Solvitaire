//
// Created by thecharlesblake on 10/24/17.
//

#include <vector>
#include <ostream>
#include <numeric>
#include <random>
#include <algorithm>
#include <functional>

#include <rapidjson/document.h>

#include "game_state.h"
#include "pile.h"

using namespace rapidjson;
using namespace std;

typedef sol_rules::build_order ord;
typedef sol_rules::build_policy pol;

// Construct an initial game state from a JSON doc
game_state::game_state(const Document& doc)
        : reserve(true, ord::NO_BUILD, pol::ANY_SUIT, false),
          stock(true, ord::NO_BUILD, pol::ANY_SUIT, false),
          waste(true, ord::NO_BUILD, pol::ANY_SUIT, false),
          hole(false, ord::BOTH, pol::ANY_SUIT, true, rules.max_rank),
          rules("simple-black-hole") {
    // Construct tableau piles
    assert(doc.HasMember("tableau piles"));
    const Value& json_tab_piles = doc["tableau piles"];
    assert(json_tab_piles.IsArray());

    int max_rank = 1;
    for (auto& json_tab : json_tab_piles.GetArray()) {
        pile tableau_pile(true, rules.build_ord, pol::ANY_SUIT, false);

        for (auto& json_card : json_tab.GetArray()) {
            card c(json_card.GetString());
            if (c.get_rank() > max_rank) max_rank = c.get_rank();
            tableau_pile.place(c);
        }

        tableau_piles.push_back(tableau_pile);
    }

    // Assign hole card
    assert(doc.HasMember("hole card"));
    const Value& json_h_card = doc["hole card"];
    assert(json_h_card.IsString());
    hole.place(json_h_card.GetString());
}

// Construct an initial game state from a seed
game_state::game_state(int seed, const sol_rules& s_rules) :
        reserve(true, ord::NO_BUILD, pol::ANY_SUIT, false),
        stock(true, ord::NO_BUILD, pol::ANY_SUIT, false),
        waste(true, ord::NO_BUILD, pol::ANY_SUIT, false),
        hole(false, ord::BOTH, pol::ANY_SUIT, true, s_rules.max_rank),
        rules(s_rules) {

    vector<card> deck = shuffled_deck(seed, rules.max_rank);

    // If there is a hole, move the ace of spades to it
    if (rules.hole) {
        deck.erase(find(begin(deck), end(deck), card("AS")));
        hole.place("AS");
    }

    unsigned int deckCardsUsed = 0;

    // If there is a stock, deal to it and set up a waste pile too
    if (rules.stock_size > 0) {
        for (unsigned int i = 0; i < rules.stock_size; i++) {
            stock.place(deck[i + deckCardsUsed]);
        }
        deckCardsUsed += rules.stock_size;
    }

    // If there is a reserve, deal to it
    if (rules.reserve_size > 0) {
        for (unsigned int i = 0; i < rules.reserve_size; i++) {
            reserve.place(deck[i + deckCardsUsed]);
        }
        deckCardsUsed += rules.reserve_size;
    }


    // Deal to the tableau piles (row-by-row)
    vector<card>::size_type remainingCards = deck.size() - deckCardsUsed;
    for (vector<card>::size_type i = 0; i < remainingCards; i++) {
        card c = deck[i + deckCardsUsed];

        // For the first row we must create all the tableau pile vectors
        if (i / rules.tableau_pile_count == 0) {
            pile p(true, rules.build_ord, pol::ANY_SUIT, false);
            tableau_piles.push_back(p);
        }

        // Add the randomly generated card to the tableau piles
        tableau_piles[i % rules.tableau_pile_count].place(c);
    }
    deckCardsUsed += remainingCards;
    assert(deckCardsUsed == deck.size());

    // If there are foundation piles, create the relevant pile vectors
    if (rules.foundations) {
        for (int i = 0; i < 4; i++) {
            pile p(true, ord::ASCENDING, pol(i), false);
            foundations.push_back(p);
        }
    }

    // If there are cell piles, create the relevant cell vectors
    if (rules.cells > 0) {
        for (unsigned int i = 0; i < rules.cells; i++) {
            pile p(true, ord::SINGLE_CARD, pol::ANY_SUIT, false);
            cells.push_back(p);
        }
    }
}

// Generate a randomly ordered vector of cards
vector<card> game_state::shuffled_deck(int seed, int max_rank = 13) {
    vector<int*> v;
    for (int i = 0; i < max_rank * 4; i++) {
        v.push_back(new int(i));
    }

    // Randomly shuffle the pointers
    auto rng = std::default_random_engine(seed);
    std::shuffle(std::begin(v), std::end(v), rng);

    vector<card> deck;
    for (int *i : v) {
        int r = ((*i) % max_rank) + 1;
        int s = (*i) / max_rank;

        deck.push_back(card(r, s));
    }

    // release memory
    for (int i = 0; i < max_rank * 4; i++) {
        delete v[i];
    }

    return deck;
}

vector<game_state> game_state::get_next_legal_states() const {
    // Generates vectors of references to all the piles from which
    // cards can be removed, and to which they can be added
    vector<pile*> can_remove;
    vector<pile*> can_add;

    // A mutable version of the current state, used to create the next legal
    // states
    game_state mut_state = *this;

    for (vector<pile>::size_type i = 0; i < tableau_piles.size(); i++) {
        can_remove.push_back(&mut_state.tableau_piles[i]);
        if (rules.build_ord != ord::NO_BUILD) {
            can_add.push_back(&mut_state.tableau_piles[i]);
        }
    }
    if (rules.reserve_size > 0) {
        can_remove.push_back(&mut_state.reserve);
    }
    if (rules.foundations) {
        for (vector<pile>::size_type i = 0; i < foundations.size(); i++) {
            can_remove.push_back(&mut_state.foundations[i]);
            can_add.push_back(&mut_state.foundations[i]);
        }
    }
    for (vector<pile>::size_type i = 0; i < cells.size(); i++) {
        can_remove.push_back(&mut_state.cells[i]);
        can_add.push_back(&mut_state.cells[i]);
    }
    if (rules.stock_size > 0) {
        can_remove.push_back(&mut_state.waste);
    }
    if (rules.hole) {
        can_add.push_back(&mut_state.hole);
    }

    // The next legal states
    vector<game_state> next;

    // Moving from and to the 'can remove' and 'can add' piles
    for (pile *rem_pile : can_remove) {
        if (rem_pile->empty()) continue;

        for (pile *add_pile : can_add) {
            if (add_pile != rem_pile
                && add_pile->can_place(rem_pile->top_card())) {

                move(rem_pile, add_pile); // remove
                game_state s(mut_state);
                next.push_back(s);
                move(add_pile, rem_pile); // restore
            }
        }
    }

    // Dealing from the stock to the waste
    if (!stock.empty()) {
        move(&mut_state.stock, &mut_state.waste);
        game_state s(mut_state);
        next.push_back(s);
        move(&mut_state.waste, &mut_state.stock);
    }

    return next;
}


bool game_state::is_solved() const {
    for (auto p : foundations) {
        if (p.size() != rules.max_rank) {
            return false;
        }
    }

    if (rules.hole && hole.size() != rules.max_rank * 4) {
        return false;
    }
    return true;
}

ostream& game_state::print(ostream& stream) const {
    if (rules.foundations) {
        print_header(stream, "Foundations");
        print_foundations(stream);
    }
    if (rules.cells) {
        print_header(stream, "Cells");
        print_cells(stream);
    }
    if (rules.reserve_size > 0) {
        print_header(stream, "Reserve");
        print_pile(stream, reserve);
    }
    if (rules.tableau_pile_count > 0) {
        print_header(stream, "Tableau Piles");
        print_tableau_piles(stream);
    }
    if (rules.stock_size > 0) {
        print_header(stream, "Stock");
        print_pile(stream, stock);
        print_header(stream, "Waste");
        print_pile(stream, waste);
    }
    if (rules.hole) {
        print_header(stream, "Hole Card");
        print_hole(stream);
    }
    return stream << "===================================\n";
}

void game_state::print_header(ostream& stream, const char* header) const {
    stream << "--- " << header << " ";
    int pad = 20 - strlen(header);
    for (int i = 0; i < pad; i++) {
        stream << '-';
    }
    stream << "\n";
}

void game_state::print_foundations(ostream& stream) const {
    stream << foundations[0] << "\t"
           << foundations[1] << "\t"
           << foundations[2] << "\t"
           << foundations[3] << "\n";
}

void game_state::print_cells(ostream& stream) const {
    for (pile cell : cells) {
        if (cells.empty()) {
            stream << "[]" << "\t";
        } else {
            stream << cells[0] << "\t";
        }
    }
    stream << "\n";
}

void game_state::print_pile(ostream& stream, const pile &p) const {
    if (p.empty()) {
        stream << "[]\n";
    } else {
        for (unsigned int i = 0; i < p.size(); i++) {
            stream << p[i] << "\n";
        }
    }
}

void game_state::print_tableau_piles(ostream& stream) const {
    bool empty_row = false;
    vector<card>::size_type row_idx = 0;

    while (!empty_row) {
        empty_row = true;

        for (pile tableau_pile : tableau_piles) {
            if (tableau_pile.size() > row_idx) {
                empty_row = false;
                stream << tableau_pile[row_idx];
            }
            stream << "\t";
        }

        stream << "\n";
        row_idx++;
    }
}

void game_state::print_hole(ostream& stream) const {
    stream << hole.top_card() << "\n";
}

bool operator==(const game_state& a, const game_state& b) {
    return a.tableau_piles == b.tableau_piles
            && a.foundations == b.foundations
            && a.hole == b.hole;
}

ostream& operator <<(ostream& stream, const game_state& gs) {
    return gs.print(stream);
}