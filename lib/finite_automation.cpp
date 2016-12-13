#include "finite_automation.hpp"
#include "token.hpp"
#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

using std::set;
using std::vector;
using std::stack;
using std::make_shared;
using std::make_pair;
using std::list;
using std::unordered_map;

vector<set<char>> priority_table = {
    {
        '|',
    },
    {
        // character priority is here
    },
    {
        '^', '$',
    },

    {
        '*', '?',
    },
    {
        '(', ')',
    },
    {
        '\\',
    },
};

static unordered_map<char, int> priority_refers;

static char_match get_regex_range(char regex_char) {
  switch (regex_char) {
  case 'd':
    return {
        make_pair('0', '9'),
    };
  case 'l':
    return {
        make_pair('a', 'z'), make_pair('A', 'Z'),
    };
  }
  return {
      make_pair(regex_char, regex_char + 1),
  };
}

inline void mark_least(fa_edge *edge) {
  for (auto range : get_regex_range(edge->regex_str[0])) {
    for (auto ch = range.first; ch < range.second; ++ch) {
      edge->acceptable_chars.insert(ch);
    }
  }
}

inline int get_prioriry(char ch) {
  auto iter = priority_refers.find(ch);
  if (iter != priority_refers.end()) {
    return iter->second;
  }
  int priority(1); //  default priority is for chars
  for (int i = 0; i < priority_table.size(); ++i) {
    auto op_set = priority_table[i];
    bool broken_flag(false);
    for (auto op : op_set) {
      if (op == ch) {
        priority = i;
        broken_flag = true;
        break;
      }
    }
    if (broken_flag) {
      break;
    }
  }
  priority_refers[ch] = priority;
  return priority;
}

inline int left_reduction(int op_index, const string &regex_str) {
  int left_end(0);
  int parentheses(0);
  int char_count(0);
  while (left_end < op_index) {
    if (regex_str[left_end] == '(') {
      if (char_count > 0 && parentheses == 0) {
        break;
      }
      parentheses++;
    } else if (regex_str[left_end] == ')') {
      parentheses--;
    } else {
      if (parentheses == 0 &&
          get_prioriry(regex_str[left_end]) <= get_prioriry('a')) {
        if (char_count > 0) {
          break;
        }
      }
      if (get_prioriry(regex_str[left_end]) == get_prioriry('a')) {
        char_count++;
      }
    }
    left_end++;
  }
  std::cout << "reduction: " << regex_str.substr(0, left_end) << std::endl;
  return left_end;
}

fa_edge::fa_edge(fa_state *start, fa_state *end, string regex_str)
    : start(start), end(end), regex_str(regex_str) {}

void fa_edge::set(fa_state *start, fa_state *end, string regex_str) {
  if (this->start != start) {
    if (this->start) {
      this->start->out_edges.erase(this);
    }
    start->out_edges.insert(this);
    this->start = start;
  }
  if (this->end != end) {
    if (this->end) {
      this->end->in_edges.erase(this);
    }
    end->in_edges.insert(this);
    this->end = end;
  }
  this->regex_str = regex_str;
}

finite_automation::finite_automation() { entry = create_state(); }

void finite_automation::reset() {
  decision_state = {};
  decision_state.push(entry);
  for (auto state : statues) {
    state->decision_edges = {};
  }
  entry->decision_edges.push(entry->out_edges.begin());
}

bool encounter_split(char ch) { return ch == ' ' || ch == '$'; }

vector<token> finite_automation::match(string sentence) {
  sentence.push_back('$');
  input_str = sentence;
  string token_str;
  vector<token> results;
  int last_token_type(-1);
  // sentence.push_back('$');
  std::cout << "match sentence " << sentence << std::endl;
  int read_index(0);
  reset();
  while (read_index < sentence.length()) {
    token_str.push_back(sentence[read_index]);
    auto step_result = step();
    // std::cout << "step result:" << step_result << std::endl;
    if (encounter_split(sentence[read_index]) || step_result == '$') {
      if (last_final_state != nullptr) {
        token_str.pop_back();
        results.push_back(token(token_str, last_final_state->final_token_code));
      }
      reset();
      token_str = "";
      last_token_type = -1;
      if (step_result == '$' && !encounter_split(sentence[read_index])) {
        continue;
      }
      read_index++;
      current_index_in_input++;
      continue;
    }
    last_token_type =
        decision_state.empty() ? -1 : decision_state.top()->final_token_code;
    read_index++;
    current_index_in_input++;
  }
  // for (int i = 0; i < sentence.length(); ++i) {
  //   if (step() == "$") {
  //     if (valid) {
  //       results.push_back(token(token_str, current->final_token_code));
  //       current = entry;
  //     }
  //     token_str = "";
  //   }
  //   valid = current->isFinal;
  //   token_str.push_back(sentence[i]);
  // }
  return results;
}

char finite_automation::step() {
  int input_index = current_index_in_input;
  char identify = '$';
  last_final_state = nullptr;
  last_final_index = input_index;
  while (!decision_state.empty() && input_index < current_index_in_input + 1) {
    auto current = decision_state.top();
    if (current->final_token_code != -1) {
      if (last_final_index <= input_index) {
        last_final_index = input_index;
        last_final_state = current;
      }
    }
    char ch = input_str[input_index];
    if (current->decision_edges.top() != current->out_edges.end()) {
      auto decision_edge = *(current->decision_edges.top());
      current->decision_edges.top()++;
      if (decision_edge->acceptable_chars.find(ch) !=
          decision_edge->acceptable_chars.end()) {
        decision_state.push(decision_edge->end);
        decision_edge->end->decision_edges.push(
            decision_edge->end->out_edges.begin());
        identify = decision_edge->regex_str[0];
        input_index++;
      }
    } else {
      decision_state.top()->decision_edges.pop();
      decision_state.pop();
      input_index--;
    }
  }
  if (!decision_state.empty()) {
    std::cout << "step " << input_str.substr(0, input_index) << "\tregex "
              << identify << "\tpush token type "
              << decision_state.top()->final_token_code << std::endl;
  } else {
    identify = '$';
  }
  return identify;
}

void finite_automation::next() {
  while (!decision_state.empty()) {
  }
}

void finite_automation::split(fa_edge *edge_to_split) {
  const int length = edge_to_split->regex_str.length();

  if (length <= 1) {
    mark_least(edge_to_split);
    return;
  }

  bool no_concat(false);

  int last_parentheses_index(0);
  int left_end = left_reduction(length, edge_to_split->regex_str);
  if (edge_to_split->regex_str[0] == '(' &&
      edge_to_split->regex_str[left_end - 1] == ')' && left_end == length) {
    edge_to_split->regex_str = edge_to_split->regex_str.substr(1, length - 2);
    split(edge_to_split);
    return;
  }

  if (left_end == length) {
    no_concat = true;
    left_end = 0;
  }

  std::cout << "split: " << edge_to_split->regex_str.substr(left_end)
            << std::endl;
  int min_prio(100);
  int min_prio_index(left_end);
  int parentheses(0);
  int right_logic_char_num(0);
  for (int i = left_end; i < length; ++i) {
    auto ch = edge_to_split->regex_str[i];
    auto priority = get_prioriry(ch);
    if (ch == '(') {
      parentheses++;
      right_logic_char_num++;
    } else if (ch == ')') {
      parentheses--;
      last_parentheses_index = i;
    } else {
      if (priority == get_prioriry('a')) {
        right_logic_char_num++;
      }
    }
    if (right_logic_char_num == 1 && !no_concat) {
      priority = get_prioriry('a');
    }
    if ((parentheses == 0 || right_logic_char_num == 1) &&
        priority < min_prio) {
      if (!no_concat || (no_concat && priority > get_prioriry('a'))) {
        min_prio = priority;
        min_prio_index = i;
      }
    }
  }
  if (min_prio == get_prioriry('a')) {
    connection(edge_to_split, left_end);
    return;
  }

  char split_rule = edge_to_split->regex_str[min_prio_index];

  // TODO(Daniel): fill all ops;
  switch (split_rule) {
  case '|':
    parallel(edge_to_split, min_prio_index);
    break;
  case '?':
    optional(edge_to_split, min_prio_index);
    break;
  case '*':
    closure(edge_to_split, min_prio_index);
    break;
  }
}

void finite_automation::parallel(fa_edge *parallel_edge, int parallel_index) {
  auto child_edge_1 = parallel_edge;
  auto child_edge_2 =
      create_edge(parallel_edge->start, parallel_edge->end,
                  parallel_edge->regex_str.substr(parallel_index + 1));
  child_edge_1->regex_str = parallel_edge->regex_str.substr(0, parallel_index);
  split(child_edge_1);
  split(child_edge_2);
}

void finite_automation::closure(fa_edge *origin, int closure_mark_index) {
  std::cout << "closure:" << origin->regex_str << std::endl;
  auto sub_regex = origin->regex_str.substr(0, closure_mark_index);
  auto closure_state = create_state();
  create_edge(origin->start, closure_state, "");
  auto leave_closure = origin;
  leave_closure->set(closure_state, origin->end, "");
  auto closure_edge = create_edge(closure_state, closure_state, sub_regex);
  std::cout << "closure result:" << closure_edge->regex_str << std::endl;
  split(closure_edge);
}

void finite_automation::optional(fa_edge *option_edge, int option_mark_index) {
  std::cout << "optionnal:" << option_edge->regex_str << std::endl;
  option_edge->regex_str = option_edge->regex_str.substr(0, option_mark_index);
  create_edge(option_edge->start, option_edge->end, "");
  split(option_edge);
}

void finite_automation::connection(fa_edge *connect_edge,
                                   int right_start_index) {
  std::cout << "connection: " << connect_edge->regex_str << std::endl;
  auto right_regex = connect_edge->regex_str.substr(right_start_index);
  auto left_regex = connect_edge->regex_str.substr(0, right_start_index);
  auto left_state = create_state();
  auto child_left_edge = connect_edge;
  auto child_right_edge =
      create_edge(left_state, connect_edge->end, right_regex);
  child_left_edge->set(connect_edge->start, left_state, left_regex);
  split(child_left_edge);
  split(child_right_edge);
}

fa_edge *finite_automation::create_edge(fa_state *start, fa_state *end,
                                        string regex_str) {
  auto edge = make_shared<fa_edge>(start, end, regex_str);
  start->out_edges.insert(edge.get());
  end->in_edges.insert(edge.get());
  edges.push_back(edge);
  return edge.get();
}

fa_state *finite_automation::create_state() {
  auto state = make_shared<fa_state>();
  statues.push_back(state);
  return state.get();
}

void finite_automation::dfs() {
  std::cout << "=====dfs start=====" << std::endl;
  list<fa_state *> que;
  unordered_set<fa_state *> states;
  que.push_back(entry);
  states.insert(entry);
  while (!que.empty()) {
    auto current = que.front();
    que.pop_front();
    for (auto out_iter = current->out_edges.begin();
         out_iter != current->out_edges.end(); ++out_iter) {
      auto out = *out_iter;
      std::cout << "dfs: " << out->regex_str;
      if (out->end->final_token_code != -1) {
        std::cout << " reach final " << out->end->final_token_code;
      }
      std::cout << std::endl;
      if (states.find(out->end) == states.end()) {
        que.push_back(out->end);
        states.insert(out->end);
      }
    }
  }
  std::cout << "=====dfs end=====" << std::endl;
}

void finite_automation::make_deterministic() {
  for (auto start : statues) {
    // if (states_set.find(start.get()) != states_set.end()) {
    //   continue;
    // }
    // bool valid(false);
    // for (auto in_edge : start->in_edges) {
    //   if (in_edge->regex_str.length() > 0) {
    //     valid = true;
    //     break;
    //   }
    // }
    // if (!valid) {
    //   break;
    // }
    list<fa_state *> que;
    unordered_set<fa_state *> states;
    que.push_back(start.get());
    states.insert(start.get());
    while (!que.empty()) {
      auto current = que.front();
      que.pop_front();
      for (auto out_iter = current->out_edges.begin();
           out_iter != current->out_edges.end(); out_iter++) {
        auto out = *out_iter;
        if (out->regex_str == "") {
          current->final_token_code = out->end->final_token_code;
          auto out_edges = out->end->out_edges;
          for (auto out_out_edge : out_edges) {
            split(create_edge(start.get(), out_out_edge->end,
                              out_out_edge->regex_str));
          }
          if (states.find(out->end) == states.end()) {
            que.push_back(out->end);
            states.insert(out->end);
          }
        }
      }
    }
  }
  for (auto edge : edges) {
    if (edge->regex_str == "") {
      edge->start->out_edges.erase(edge.get());
      edge->end->in_edges.erase(edge.get());
    }
  }
}

void finite_automation::add_regular(string regular_expression, int token_code) {
  auto end = create_state();
  end->final_token_code = token_code;
  end->isFinal = true;
  auto first_edge = create_edge(entry, end, regular_expression);
  split(first_edge);
}