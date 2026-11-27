#ifndef __bsp3_room_allocator_hpp__
#define __bsp3_room_allocator_hpp__

#include <optional>
#include <tuple>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include "cadical.hpp" /* XXX: fix path */


namespace bsp3 {
  

typedef int RequestId;
typedef int RoomId;
typedef int Date;


struct Request {
  RequestId request_id;
  int num_people;
  int length_of_stay;
};


struct Room {
  RoomId room_id;
  int capacity;
};


struct Decision {
  RequestId request_id;
  RoomId room_id;
  Date start_date;
  Date end_date;
};


class RoomAllocator : public CaDiCaL::ExternalPropagator {

private:
  // typedef std::tuple<RequestId, RoomId, Date> QrdTuple;
  // typedef std::tuple<RequestId, RoomId> QrTuple;

  struct QrTuple {
    int request_id;
    int room_id;

    int
    operator<=> (const QrTuple &rhs)
    const
    {
      const QrTuple &lhs = *this;
      int sgn;

      sgn = lhs.request_id - rhs.request_id;
      if (sgn)
        return sgn;

      sgn = lhs.room_id - rhs.room_id;
      return sgn;
    }
  };

  struct QrdTuple {
    RequestId request_id;
    RoomId room_id;
    int date;

    int
    operator<=> (const QrdTuple &rhs)
    const
    {
      const QrdTuple &lhs = *this;
      int sgn;

      sgn = lhs.request_id - rhs.request_id;
      if (sgn)
        return sgn;

      sgn = lhs.room_id - rhs.room_id;
      if (sgn)
        return sgn;

      sgn = lhs.date - rhs.date;
      return sgn;
    }
  };


  struct RequestMarker {
    RoomId room_id;
    std::vector<RoomId> excluded_rooms; /* shrinks */
    size_t excluded_rooms_idx = 0;

    std::optional<Date> start_date;
    std::optional< std::vector<Date> > excluded_dates; /* shrinks */
    size_t excluded_dates_idx = 0;

    bool
    is_finished ()
    const
    {
      if (excluded_rooms_idx < excluded_rooms.size())
        return false;

      if (! excluded_dates.has_value() )
        return false;

      if (excluded_dates.has_value()
       && excluded_dates_idx < excluded_dates->size())
        return false;
      
      return true;
    }
  };

public:
  explicit
  RoomAllocator (const std::vector<Room> &rooms,
                 const std::vector<Request> &requests,
                 int days);
  int run ();
  std::vector<Decision> decode() const;

protected:
  /*
   * CaDiCaL overrides
   */
  void notify_assignment (const std::vector<int> &lits) override;
  void notify_new_decision_level () override;
  void notify_backtrack (size_t new_level) override;
  bool cb_check_found_model (const std::vector<int> &model) override;
  bool cb_has_external_clause (bool &is_forgettable) override;
  int cb_add_external_clause_lit () override;
  int cb_add_reason_clause_lit (int plit) override;
  int cb_propagate () override;
  int cb_decide () override;

private:
  /*
   * Private
   */
  void register_start_ ();
  void flush_observed_ ();

  int new_top_var_ (RoomAllocator::QrTuple qr);
  int maybe_new_norm_var_ (RoomAllocator::QrdTuple qrd);
  int maybe_new_start_var_ (RoomAllocator::QrdTuple qrd);

  std::string pr_ (int v) const;
  RequestMarker &notify_marker_ (RequestId request_id, RoomId room_id);

private:
  /*
   * Library user input
   */
  std::map<RoomId, Room> rooms_;
  std::map<RequestId, Request> requests_;
  int days_ = -1;

private:
  /*
   * CaDiCaL bookkeeping
   */
  CaDiCaL::Solver slv_;
  int var_counter_ = 0;

  std::vector<int> pending_observed_;

  std::vector<int> reason_clause_;
  size_t reason_idx_ = 0;
  bool reason_active_ = false;

  std::vector<int> trail_;
  std::vector<size_t> level_lims_;

  std::vector<std::vector<int>> pending_clauses_;
  std::vector<int> ext_clause_;
  size_t ext_idx_ = 0;

private:
  /*
   * Domain-specific
   */
  std::map<QrTuple, int> vars_top_enc_;
  std::map<QrdTuple, int> vars_norm_enc_;
  std::map<QrdTuple, int> vars_start_enc_;

  std::unordered_map<int, QrTuple> vars_top_dec_;
  std::unordered_map<int, QrdTuple> vars_norm_dec_;
  std::unordered_map<int, QrdTuple> vars_start_dec_;

  std::set<int> true_top_;
  std::set<int> true_norm_;
  std::set<int> true_start_;

  std::unordered_map<RequestId, RequestMarker> request_markers_;
  // std::queue<RequestId> unfinished_markers_;
  std::set<RequestId> unfinished_markers_;

private:
  bool running_;
  bool done_;
  bool success_;
};


}
#endif /*__bsp3_room_allocator_hpp__*/

