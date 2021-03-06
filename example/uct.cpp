/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 *                                                                           *
 *  This file is part of Library of Effective GO routines - EGO library      *
 *                                                                           *
 *  Copyright 2006 and onwards, Lukasz Lew                                   *
 *                                                                           *
 *  EGO library is free software; you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation; either version 2 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  EGO library is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with EGO library; if not, write to the Free Software               *
 *  Foundation, Inc., 51 Franklin St, Fifth Floor,                           *
 *  Boston, MA  02110-1301  USA                                              *
 *                                                                           *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// uct parameters

const float initial_value                 = 0.0;
const float initial_bias                  = 1.0;
const float mature_bias_threshold         = initial_bias + 100.0;
const float explore_rate                  = 1.0;
const uint  uct_max_depth                 = 1000;
const uint  uct_max_nodes                 = 1000000;
const float resign_value                  = 0.95;
const uint  uct_genmove_playout_cnt       = 50000;
const float print_visit_threshold_base    = 500.0;
const float print_visit_threshold_parent  = 0.02;

//class Stat

class Stat {
public:
  float sample_count;
  float sample_sum;
  float square_sample_sum;

  Stat () {
    reset ();
  }
  
  void reset (float prior_sample_count = 1.0) {
    sample_count       = prior_sample_count; // TODO 
    sample_sum         = 0.0; // TODO
    square_sample_sum  = 0.0; // TODO
  }

  void update (float sample) {
    sample_count       += 1.0;
    sample_sum         += sample;
    square_sample_sum  += sample * sample;
  }

  float mean () { 
    return sample_sum / sample_count; 
  }

  float variance () {
    // VX = E(X^2) - EX ^ 2
    float m = mean ();
    return square_sample_sum / sample_count - m * m;
  }

  float std_dev () { return sqrt (variance ()); }
  float std_err () { return sqrt (variance () / sample_count); } // TODO assert sample_count
                                                                 

  float get_sample_count () { return sample_count; }

  string to_string (bool dont_show_unupdated = true) {
    if (dont_show_unupdated && sample_count < 2.0) return "           ";

    ostringstream out;
    char buf [100];
    sprintf (buf, "%+3.1f(%5.0f)", mean (), sample_count);
    out << buf;
    return out.str ();
  }

};



// class Node


class Node {

public:

  Vertex     v;
  
  // TODO this should be replaced by Stat
  float    value;
  float    bias;

  FastMap<Player, Node*> first_child;         // head of list of moves of particular player 
  Node*  sibling;                           // NULL if last child

public:
  
  #define node_for_each_child(node, pl, act_node, i) do {   \
    assertc (tree_ac, node!= NULL);                         \
    Node* act_node;                                         \
    act_node = node->first_child [pl];                      \
    while (act_node != NULL) {                              \
      i;                                                    \
      act_node = act_node->sibling;                         \
    }                                                       \
  } while (false)

  void check () const {
    if (!tree_ac) return;
    assert (bias >= 0.0);
    v.check ();
  }
  
  void init (Vertex v) {
    this->v       = v;
    value         = initial_value;
    bias          = initial_bias;

    player_for_each (pl) 
      first_child [pl]  = NULL;

    sibling       = NULL;
  }

  void add_child (Node* new_child, Player pl) { // TODO sorting?
    assertc (tree_ac, new_child->sibling     == NULL); 
    player_for_each (pl2)
      assertc (tree_ac, new_child->first_child [pl2] == NULL); 

    new_child->sibling     = this->first_child [pl];
    this->first_child [pl] = new_child;
  }

  void remove_child (Player pl, Node* del_child) { // TODO inefficient
    Node* act_child;
    assertc (tree_ac, del_child != NULL);

    if (first_child [pl] == del_child) {
      first_child [pl] = first_child [pl]->sibling;
      return;
    }
    
    act_child = first_child [pl];

    while (true) {
      assertc (tree_ac, act_child != NULL);
      if (act_child->sibling == del_child) {
        act_child->sibling = act_child->sibling->sibling;
        return;
      }
      act_child = act_child->sibling;
    }
  }

  float ucb (Player pl, float explore_coeff) {  // TODO pl_idx is awfull
    return 
      (pl == Player::black () ? value : -value) +
      sqrt (explore_coeff / bias);
  }

  void update (float result) {
    bias  += 1.0;
    value += (result - value) / bias; // TODO inefficiency ?
  }

  bool is_mature () { 
    return bias > mature_bias_threshold; 
  }

  bool no_children (Player pl) {
    return first_child [pl] == NULL;
  }

  Node* find_uct_child (Player pl) {
    Node* best_child;
    float   best_urgency;
    float   explore_coeff;

    best_child     = NULL;
    best_urgency   = - large_float;
    explore_coeff  = log (bias) * explore_rate;

    node_for_each_child (this, pl, child, {
      float child_urgency = child->ucb (pl, explore_coeff);
      if (child_urgency > best_urgency) {
        best_urgency  = child_urgency;
        best_child    = child;
      }
    });

    assertc (tree_ac, best_child != NULL); // at least pass
    return best_child;
  }

  Node* find_most_explored_child (Player pl) {
    Node* best_child;
    float   best_bias;

    best_child     = NULL;
    best_bias      = -large_float;
    
    node_for_each_child (this, pl, child, {
      if (child->bias > best_bias) {
        best_bias     = child->bias;
        best_child    = child;
      }
    });

    assertc (tree_ac, best_child != NULL);
    return best_child;
  }

  void rec_print (ostream& out, uint depth, Player pl) {
    rep (d, depth) out << "  ";
    out 
      << pl.to_string () << " " 
      << v.to_string () << " " 
      << value << " "
      << "(" << bias - initial_bias << ")" 
      << endl;

    player_for_each (pl)
      rec_print_children (out, depth, pl);
  }

  void rec_print_children (ostream& out, uint depth, Player player) {
    Node*  child_tab [Vertex::cnt]; // rough upper bound for the number of legal move
    uint     child_tab_size;
    uint     best_child_idx;
    float    min_visit_cnt;
    
    child_tab_size  = 0;
    best_child_idx  = 0;
    min_visit_cnt   =
      print_visit_threshold_base + 
      (bias - initial_bias) * print_visit_threshold_parent; 
    // we want to be visited at least initial_bias times + 
    // some percentage of parent's visit_cnt

    // prepare for selection sort
    node_for_each_child (this, player, child, child_tab [child_tab_size++] = child);

    #define best_child child_tab [best_child_idx]

    while (child_tab_size > 0) {
      // find best child
      rep(ii, child_tab_size) {
        if ((player == Player::black ()) == 
            (best_child->value < child_tab [ii]->value))
          best_child_idx = ii;
      }
      // rec call
      if (best_child->bias - initial_bias >= min_visit_cnt)
        child_tab [best_child_idx]->rec_print (out, depth + 1, player);      
      else break;

      // remove best
      best_child = child_tab [--child_tab_size];
    }

    #undef best_child
    
  }
};



// class Tree


class Tree {

public:

  FastPool <Node> node_pool;
  Node*           history [uct_max_depth];
  uint            history_top;

public:

  Tree () : node_pool(uct_max_nodes) {
    history [0] = node_pool.malloc ();
    history [0]->init (Vertex::any ());
  }

  void history_reset () {
    history_top = 0;
  }
  
  Node* act_node () {
    return history [history_top];
  }
  
  void uct_descend (Player pl) {
    history [history_top + 1] = act_node ()->find_uct_child (pl);
    history_top++;
    assertc (tree_ac, act_node () != NULL);
  }
  
  void alloc_child (Player pl, Vertex v) {
    Node* new_node;
    new_node = node_pool.malloc ();
    new_node->init (v);
    act_node ()->add_child (new_node, pl);
  }
  
  void delete_act_node (Player pl) {
    assertc (tree_ac, history_top > 0);
    history [history_top-1]->remove_child (pl, act_node ());
    node_pool.free (act_node ());
  }
  
  void free_subtree (Node* parent) {
    player_for_each (pl) {
      node_for_each_child (parent, pl, child, {
        free_subtree (child);
        node_pool.free (child);
      });
    }
  }

  // TODO free history (for sync with base board)
  
  void update_history (float sample) {
    rep (hi, history_top+1) 
      history [hi]->update (sample);
  }

  string to_string () { 
    ostringstream out_str;
    history [0]->rec_print (out_str, 0, Player::black ()); 
    return out_str.str ();
  }
};


 // class Uct


class Uct {
public:
  
  Board&        base_board;
  Tree          tree[1];      // TODO tree->root should be in sync with top of base_board
  
public:
  
  Uct (Board& base_board_) : base_board (base_board_) { }

  void root_ensure_children_legality (Player pl) { // cares about superko in root (only)
    tree->history_reset ();

    assertc (uct_ac, tree->history_top == 0);
    assertc (uct_ac, tree->act_node ()->first_child [pl] == NULL);

    empty_v_for_each_and_pass (&base_board, v, {
      if (base_board.is_strict_legal (pl, v))
        tree->alloc_child (pl, v);
    });
  }

  flatten 
  void do_playout (Player first_player){
    Board    play_board[1]; // TODO test for perfomance + memcpy
    FastMap<Player, bool> was_pass;
    Player  act_player = first_player;
    Vertex       v;
    
    
    play_board->load (&base_board);
    tree->history_reset ();
    
    player_for_each (pl) 
      was_pass [pl] = false; // TODO maybe there was one pass ?
    
    do {
      if (tree->act_node ()->no_children (act_player)) { // we're finishing it
        
        // If the leaf is ready expand the tree -- add children - all potential legal v (i.e.empty)
        if (tree->act_node ()->is_mature ()) {
          empty_v_for_each_and_pass (play_board, v, {
            tree->alloc_child (act_player, v); // TODO simple ko should be handled here
            // (suicides and ko recaptures, needs to be dealt with later)
          });
          continue;            // try again
        }
        
        // TODO assert act_plauer == board->Act_player ()
        SimplePolicy policy;
        Playout<SimplePolicy> (&policy, play_board).run ();
        break;
        
      }
      
      tree->uct_descend (act_player);
      v = tree->act_node ()->v;
      
      if (play_board->is_pseudo_legal (act_player, v) == false) {
        assertc (uct_ac, tree->act_node ()->no_children (act_player.other ()));
        tree->delete_act_node (act_player);
        return;
      }
      
      play_board->play_legal (act_player, v);

      if (play_board->last_move_status != Board::play_ok) {
        assertc (uct_ac, tree->act_node ()->no_children (act_player.other ()));
        tree->delete_act_node (act_player);
        return;
      }

      was_pass [act_player]  = (v == Vertex::pass ());
      act_player             = act_player.other ();
      
      if (was_pass [Player::black ()] & was_pass [Player::white ()]) break;
      
    } while (true);
    
    int winner_idx = play_board->winner ().get_idx ();
    tree->update_history (1 - winner_idx - winner_idx); // result values are 1 for black, -1 for white
  }
  

  Vertex genmove (Player player) {
    Node* best;

    root_ensure_children_legality (player);

    rep (ii, uct_genmove_playout_cnt) do_playout (player);
    best = tree->history [0]->find_most_explored_child (player);
    assertc (uct_ac, best != NULL);

    cerr << tree->to_string () << endl;
    if (player == Player::black () && best->value < -resign_value ||
        player == Player::white () && best->value >  resign_value) {
      return Vertex::resign ();
    }
    return best->v;
  }
  
};
