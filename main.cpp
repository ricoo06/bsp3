// Template: CaDiCaL IPASIR-UP user propagator (ExternalPropagator).
//
// Example constraint implemented by the propagator:
//   "at most 2 of the variables 1..5 may be true".
//
// Targets CaDiCaL 2.x.
//
// Unfortunately largely generated, but manually checked to make sure it doesn't
// contain nonsense.

#include "cadical.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

class AtMostTwo : public CaDiCaL::ExternalPropagator {
public:
  explicit AtMostTwo(std::vector<int> vars) : vars(std::move(vars)) {
    // is_lazy = true;  // only call cb_propagate etc. on full assignments
    // are_reasons_forgettable = true;
  }

  // --- Tracking the assignment (trail with decision-level limits) ---------

  // Called with all newly assigned *watched* literals (also fixed ones).
  void notify_assignment(const std::vector<int> &lits) override {
    for (int l : lits)
      trail.push_back(l);
  }

  void notify_new_decision_level() override {
    level_lims.push_back(trail.size());
  }

  void notify_backtrack(size_t new_level) override {
    if (new_level < level_lims.size()) {
      trail.resize(level_lims[new_level]);
      level_lims.resize(new_level);
    }
  }

  // --- Final model check --------------------------------------------------

  // Return true to accept the model. Return false and queue a clause
  // (via cb_has_external_clause) to reject it.
  bool cb_check_found_model(const std::vector<int> &model) override {
    std::cout << "check model" << std::endl;
    
    std::vector<int> true_vars;
    for (int v : vars)
      if (std::find(model.begin(), model.end(), v) != model.end())
        true_vars.push_back(v);
    if (true_vars.size() <= 2)
      return true;
    std::vector<int> clause;
    for (int i = 0; i < 3; i++)
      clause.push_back(-true_vars[i]);
    pending_clauses.push_back(std::move(clause));
    return false;
  }

  // --- Adding external clauses -------------------------------------------

  // Asked repeatedly; return true while there is a clause to add.
  bool cb_has_external_clause(bool &is_forgettable) override {
    is_forgettable = false;
    if (pending_clauses.empty())
      return false;
    ext_clause = std::move(pending_clauses.back());
    pending_clauses.pop_back();
    ext_idx = 0;
    return true;
  }

  // Literals of the clause one by one, terminated by 0.
  int cb_add_external_clause_lit() override {
    if (ext_idx < ext_clause.size())
      return ext_clause[ext_idx++];
    return 0;
  }

  // --- Propagation --------------------------------------------------------

  // Return a literal to be propagated, or 0 for none.
  int cb_propagate() override {
    std::vector<int> t = true_vars();
    if (t.size() < 2)
      return 0;
    for (int v : vars) {
      if (!is_assigned(v)) {
        reasons[-v] = {t[0], t[1]};
        return -v;
      }
    }
    return 0;
  }

  // Reason clause for a literal we propagated: (lit | -a | -b), then 0.
  int cb_add_reason_clause_lit(int plit) override {
    if (!reason_active) {
      auto p = reasons.at(plit);
      reason_clause = {plit, -p.first, -p.second};
      reason_idx = 0;
      reason_active = true;
    }
    if (reason_idx < reason_clause.size())
      return reason_clause[reason_idx++];
    reason_active = false;
    return 0;
  }

  // --- Decisions (optional) ----------------------------------------------

  // Return a literal to decide on, or 0 to let CaDiCaL decide.
  int cb_decide() override { return 0; }

private:
  bool is_assigned(int v) const {
    for (int l : trail)
      if (std::abs(l) == v)
        return true;
    return false;
  }

  std::vector<int> true_vars() const {
    std::vector<int> r;
    for (int l : trail)
      if (l > 0 && std::find(vars.begin(), vars.end(), l) != vars.end())
        r.push_back(l);
    return r;
  }

  std::vector<int> vars;
  std::vector<int> trail;
  std::vector<size_t> level_lims;

  std::vector<std::vector<int>> pending_clauses;
  std::vector<int> ext_clause;
  size_t ext_idx = 0;

  std::map<int, std::pair<int, int>> reasons;
  std::vector<int> reason_clause;
  size_t reason_idx = 0;
  bool reason_active = false;
};

int main() {
  CaDiCaL::Solver solver;

  // Ordinary clauses: (1 | 2) & (3 | 4) & (4 | 5)
  for (auto c : std::vector<std::vector<int>>{{1, 2}, {3, 4}, {4, 5}}) {
    for (int l : c)
      solver.add(l);
    solver.add(0);
  }

  // Connect the propagator and declare which variables it observes.
  std::vector<int> observed = {1, 2, 3, 4, 5};
  AtMostTwo propagator(observed);
  solver.connect_external_propagator(&propagator);
  for (int v : observed)
    solver.add_observed_var(v);

  int res = solver.solve();
  std::cout << (res == 10   ? "SATISFIABLE"
                : res == 20 ? "UNSATISFIABLE"
                            : "UNKNOWN")
            << std::endl;

  if (res == 10) {
    int count = 0;
    for (int v : observed) {
      bool val = solver.val(v) > 0;
      count += val;
      std::cout << "  x" << v << " = " << val << std::endl;
    }
    std::cout << "  true vars: " << count << " (must be <= 2)" << std::endl;
  }

  // Disconnect before the propagator goes out of scope.
  solver.disconnect_external_propagator();
  return res == 10 || res == 20 ? 0 : 1;
}
