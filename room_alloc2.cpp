/*
 * license wow
 *
 * First iteration of our solver program
 * This might be hella slow, we don't know yet.
 */
#include <algorithm>
#include <tuple>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <iostream>

#include "cadical.hpp"

/*
 * These typedefs will be available for the library users
 */
typedef int RequestId;
typedef int RoomId;
typedef int Date;


struct Request {
  RequestId request_id;
  int num_people;
  int length_of_stay;
#if 0
  std::vector<int> lengths_of_stay;
#endif
};


struct Room {
  RoomId room_id;
  int capacity;
#if 0
  std::vector<RoomId> adjacent;
#endif
};


/*
 * OUTPUT
 */
struct Decision {
  RequestId request_id;
  RoomId room_id;
  Date start_date;
  Date end_date;
};


/*
 * INTERNAL
 */
typedef std::tuple<RequestId, RoomId, Date> QrdTuple;
typedef std::tuple<RequestId, RoomId> QrTuple;

 
class RoomAllocator : public CaDiCaL::ExternalPropagator {
public:
  explicit
  RoomAllocator (const std::vector<Room> &rooms,
                 const std::vector<Request> &requests,
                 int days)
  {
    for (const Room &r : rooms)
      rooms_[r.room_id] = r;

    for (const Request &r : requests)
      requests_[r.request_id] = r;

    days_ = days;
  }

public:
  void
  register_start (CaDiCaL::Solver &slv)
  {
    std::map<RoomId, std::vector<int>> room_var_lists;
    slv_ = &slv;

    for (auto it_req : requests_) {
      RequestId request_id = it_req.first;
      const Request &request = it_req.second;

      std::vector<int> req_top_vars;
      
      for (auto it_room : rooms_) {
        RoomId room_id = it_room.first;
        const Room &room = it_room.second;

        if (room.capacity < request.num_people)
          continue;

        QrTuple qr = {request_id, room_id};
        int qrv = new_top_var_(qr);
        req_top_vars.push_back(qrv);

        QrdTuple qrd = {request_id, room_id, 0};
        int qrdsv = maybe_new_start_var_(qrd);
        int qrdv = maybe_new_var_(qrd);

        room_var_lists[room_id].push_back(qrdv);

        // qrdsv => qrdv
        slv.add(-qrdsv);
        slv.add(qrdv);
        slv.add(0);
        // qrdsv => qrv
        slv.add(-qrdsv);
        slv.add(qrv);
        slv.add(0);

        /* XXX: forward calendar? */
      }

      for (int qrv : req_top_vars)
        slv.add(qrv);
      slv.add(0);
    }

    for (auto it_rv : room_var_lists) {
      std::vector<int> &vars = it_rv.second;

      for (int v : vars)
        slv.add(v);
      slv.add(0);
    }

  }

  void
  flush_observed ()
  {
    for (int v : pending_observed_)
      slv_->add_observed_var(v);
      
    pending_observed_.clear();
  }

  std::vector<Decision>
  decode ()
  const
  {
#if 1
    std::vector<Decision> decisions;

    for (auto it_req : requests_) {
      RequestId req_id = it_req.first;
      const Request &req = it_req.second;
      
      for (auto it_room : rooms_) {
        RoomId room_id = it_room.first;
        Room &room = it_room.second;

        if (req.num_people > room.capacity)
          continue;

        QrTuple qr = {req_id, room_id};
        int qrv = vars_top_enc_.at(qr);

        if (slv_->val(qrv) <= 0)
          continue;

        for (auto it_start : vars_start_enc_) {
          QrdTuple t = it_start.first;
          int tv = it_start.second;
          RequestId t_req_id = std::get<0>(t);
          RoomId t_room_id = std::get<1>(t);
          Date t_date = std::get<2>(t);
          
          if (t_req_id != req_id || t_room_id != room_id)
            continue;

          if (slv_->val(tv) <= 0)
            continue;

          Date start_date = t_date;
          Date end_date = t_date + req.length_of_stay - 1;
          Decision de = {req_id, room_id, start_date, end_date};
          decisions.push_back(std::move(de));
          break;
        }
        // abort();
      }
    }
    
    return decisions;
#else
    return decisions_;
#endif
  }

public:
  void
  notify_assignment (const std::vector<int> &lits)
  override
  {
    for (int l : lits) {
      // std::cout << "new assignment: " << l << std::endl;
      trail_.push_back(l);
    }
  }

  void
  notify_new_decision_level ()
  override
  {
    std::cout << "notify new decision level" << std::endl;
    level_lims_.push_back(trail_.size());
  }

  void
  notify_backtrack (size_t new_level)
  override
  {
    std::cout << "notifying backtrack" << std::endl;
    if (new_level < level_lims_.size()) {
      trail_.resize(level_lims_[new_level]);
      level_lims_.resize(new_level);
    }
  }

  bool
  cb_check_found_model (const std::vector<int> &model)
  override
  {
    vars_norm_true_.clear();
    vars_start_true_.clear();
    decisions_.clear();
    seen_requests_.clear();
    
    /*
     * Get assignments
     */
    for (int v : model) {
      if (vars_norm_dec_.contains(v)) {
        // std::cout << "n: " << v << std::endl;
        vars_norm_true_.insert(v);
      } else if (vars_start_dec_.contains(v)) {
        // std::cout << "s: " << v << std::endl;
        vars_start_true_.insert(v);
      } else if (vars_top_dec_.contains(v)) {
        vars_top_true_.insert(v);
      }
    }

    for (auto it_req : requests_) {
      RequestId req_id = it_req.first;

      for (auto it_room : rooms_) {
        ROomId room_id = it_room.first;
      }
    }

   
    /*
     * Generate decisions
     */
     for (int v : vars_start_true_) {
       QrdTuple t = vars_start_dec_.at(v);

       const RequestId req_id = std::get<0>(t);
       const RoomId room_id = std::get<1>(t);
       const Date date = std::get<2>(t);

       for (Decision &d : decisions_) {
         /*
          * Bail out if duplicate booking
          * XXX: does this cover QR ^ Q'R cases?
          */
         std::cout << "HI BRO" << std::endl;
         
         if (d.request_id == req_id) {
           for (int i = date; i < date + requests_[req_id].length_of_stay; ++i) {
             for (int j = d.start_date; j < d.end_date; ++j) {
               QrdTuple ti = {req_id, room_id, i};
               QrdTuple tj = {req_id, d.room_id, j};
               int vi = maybe_new_var_(ti);
               int vj = maybe_new_var_(tj);

               pending_clauses_.emplace_back(std::vector<int>{-vi, vj});
               pending_clauses_.emplace_back(std::vector<int>{-vj, vi});
             }
           }

           flush_observed();
           return false;
         }

       }

       const Room &room = rooms_.at(room_id);
       const Request &req = requests_.at(req_id);

       /*
        * Check if valid length of stay
        */
       if (date + req.length_of_stay >= days_) {
         /*
          * Fail immediately: this interval extends beyond the calendar
          * Exclude all following start vars?
          */
         for (Date d = date; d < days_; ++d) {
           const QrdTuple sdt = {req_id, room_id, d};
           const int sdv = maybe_new_start_var_(sdt);

           pending_clauses_.emplace_back(std::vector<int>{-sdv});
         }

         flush_observed();
         return false;
       }

       const QrdTuple st = {req_id, room_id, date};
       const int sv = maybe_new_start_var_(st); /* start variable */

       bool missing_in_chain = false;
       for (Date d = date; d <= date + req.length_of_stay - 1; ++d) {
         QrdTuple ct = {req_id, room_id, d};
         int v = maybe_new_var_(ct);

         if ( !vars_norm_true_.contains(v) ) {
           pending_clauses_.emplace_back(std::vector<int>{-sv, v});
           missing_in_chain = true;
         }
       }

       if (missing_in_chain) {
         flush_observed();
         return false;
       }

       Decision de = {req_id, room_id, date, date + req.length_of_stay - 1};
       decisions_.push_back(std::move(de));
     }
     
    /*
     * Find intersection of 2 bookings in a room for any date.
     * Fail if any found.
     */
    for (size_t i = 0; i < decisions_.size(); ++i) {
      for (size_t j = i + 1; j < decisions_.size(); ++j) {
        Decision &di = decisions_.at(i);
        Decision &dj = decisions_.at(j);
        
        if (dj.room_id == di.room_id
         && dj.start_date >= di.start_date
         && dj.start_date <= di.end_date) {
          const int room_id = di.room_id;
          
          for (int k = std::max(di.start_date, dj.start_date);
               k <= std::min(di.end_date, dj.end_date);
               ++k) {
            QrdTuple ti = {di.request_id, room_id, k};
            QrdTuple tj = {dj.request_id, room_id, k};
            int vi = maybe_new_var_(ti);
            int vj = maybe_new_var_(tj);

            pending_clauses_.emplace_back(std::vector<int>{-vi, vj});
            pending_clauses_.emplace_back(std::vector<int>{-vj, vi});
          }

          flush_observed();
          return false;
        }
        
      }
    }

    flush_observed();
    return true;
  }

  bool
  cb_has_external_clause (bool &is_forgettable)
  override
  {
    std::cout << "has external clause?" << std::endl;
    is_forgettable = false;
    if (pending_clauses_.empty())
      return false;

    ext_clause_ = std::move(pending_clauses_.back());
    pending_clauses_.pop_back();
    ext_idx_ = 0;
    return true;
  }

  int
  cb_add_external_clause_lit ()
  override
  {
    std::cout << "add external clause lit" << std::endl;
    if (ext_idx_ < ext_clause_.size())
      return ext_clause_[ext_idx_++];
    return 0;
  }

  int
  cb_propagate ()
  override
  {
    return 0;
  }

#if 0
  int
  cb_add_reason_clause_lit (int plit)
  override
  {
    if (! reason_active_ ) {
      /* XXX: reason clause */
      reason_idx_ = 0;
      reason_active_ = true;
    }
    if (reason_idx_ < reason_clause_.size())
      return reason_clause_[reason_idx_++];
    reason_active_ = false;
    return 0;
  }
#endif

  int
  cb_add_reason_clause_lit ([[maybe_unused]] int plit)
  {
    return 0;
  }

  /*
   * Return a literal to decide on, or 0 to let CaDiCaL decide.
   */
  int
  cb_decide ()
  override
  {
    return 0;
  }

private:
  bool
  is_assigned_ (int v)
  const
  {
    for (int l : trail_)
      if (std::abs(l) == v)
        return true;
    return false;
  }

  std::vector<int>
  true_vars_ ()
  const
  {
    std::vector<int> r;
    /* ... */
    return r;
  }

  int
  new_top_var_ (QrTuple t)
  {
    int v = ++var_counter_;
    vars_top_enc_[t] = v;
    vars_top_dec_[v] = t;
    pending_observed_.push_back(v);
    return v;
  }

  int
  maybe_new_var_ (QrdTuple t)
  {
    if (vars_norm_enc_.contains(t))
      return vars_norm_enc_.at(t);

    int v = ++var_counter_;
    vars_norm_enc_[t] = v;
    vars_norm_dec_[v] = t;
    pending_observed_.push_back(v);
    return v;
  }

  int
  maybe_new_start_var_ (QrdTuple t)
  {
    if (vars_start_enc_.contains(t))
      return vars_start_enc_.at(t);

    int v = ++var_counter_;
    vars_start_enc_[t] = v;
    vars_start_dec_[v] = t;
    pending_observed_.push_back(v);
    return v;
  }

  std::set<int>&
  maybe_new_backwards_lengths_ (QrdTuple t)
  {
    if (backwards_lengths_.contains(t))
      return backwards_lengths_.at(t);

    return backwards_lengths_[t];
  }

private:
  /*
   * Input
   */
  std::map<RoomId, Room> rooms_;
  std::map<RequestId, Request> requests_;
  int days_;


  /*
   * State
   */
  CaDiCaL::Solver *slv_;
  
  int var_counter_ = 0;

  std::vector<int> trail_;
  std::vector<size_t> level_lims_;

  std::vector<int> pending_observed_;

  std::map<QrTuple, int> vars_top_enc_;
  std::map<QrdTuple, int> vars_norm_enc_;
  std::map<QrdTuple, int> vars_start_enc_;
  std::map<QrdTuple, std::set<int>> backwards_lengths_;

  std::map<int, QrTuple> vars_top_dec_;
  std::map<int, QrdTuple> vars_norm_dec_;
  std::map<int, QrdTuple> vars_start_dec_;

  std::set<int> vars_top_true_;
  std::set<int> vars_norm_true_;
  std::set<int> vars_start_true_;

  std::vector<std::vector<int>> pending_clauses_;
  std::vector<int> ext_clause_;
  size_t ext_idx_ = 0;

  // ...
  std::vector<int> reason_clause_;
  size_t reason_idx_ = 0;
  bool reason_active_ = false;


  /*
   * Output
   */
  std::vector<Decision> decisions_;
  std::set<RequestId> seen_requests_;
};


const std::vector<Room> sample_rooms = {
  {1, 20},
  {2, 35},
  {3, 40},
  {4, 16},
};

const std::vector<Request> sample_requests = {
  {1, 20, 3},
  {2, 20, 3},
  {3, 40, 10},
  {4, 30, 5},
};

const int sample_days = 10;


int
main (int argc, char **argv)
{
  (void) argc;
  (void) argv;

  CaDiCaL::Solver slv;

  RoomAllocator prop(sample_rooms, sample_requests, sample_days);
  prop.register_start(slv);
  
  slv.connect_external_propagator(&prop);
  prop.flush_observed();

  int res = slv.solve();
  char const *status;
  switch (res) {
    case 10:
      status = "SATISFIABLE";
      break;
    case 20:
      status = "UNSATISFIABLE";
      break;
    default:
      status = "UNKNOWN";
      break;
  }

  std::cout << status << std::endl;

  if (res != 10) {
    slv.disconnect_external_propagator();
    return -1;
  }
  
  std::vector<Decision> decisions = prop.decode();
  std::cout << "Solution:" << std::endl;
  for (const Decision &de : decisions) {
    std::cout <<
    "{" << std::endl <<
    "  request_id " << de.request_id << std::endl <<
    "  room_id " << de.room_id << std::endl <<
    "  start_date " << de.start_date << std::endl <<
    "  end_date " << de.end_date << std::endl <<
    "}" << std::endl;
  }
  // std::cout << "b4 disconnect" << std::endl;
  slv.disconnect_external_propagator();
  // std::cout << "after disconnect" << std::endl;
  return 0;
}
