#include "room_allocator.hpp"
#include <iostream>
#include <format>


using namespace bsp3;
// using RoomAllocator::QrTuple;
// using RoomAllocator::QrdTuple;


RoomAllocator::RoomAllocator (const std::vector<Room> &rooms,
                              const std::vector<Request> &requests,
                              int days)
{
  for (const Room &r : rooms)
    rooms_[r.room_id] = r;

  for (const Request &r : requests)
    requests_[r.request_id] = r;

  days_ = days;
}


int
RoomAllocator::run ()
{
  register_start_();
  slv_.connect_external_propagator(this);
  flush_observed_();

  done_ = false;
  running_ = true;
  
  int ret;
  int rc = slv_.solve();

  switch (rc) {
    case 10:
      /* satisfiable */
      ret = 0;
      success_ = true;
      break;
    case 20:
      /* unsatisfiable */
      ret = -1;
      break;
    default:
      /* ??? */
      ret = +1;
      break;
  }

  slv_.disconnect_external_propagator();

  running_ = false;
  done_ = true;
  return ret;
}


std::vector<Decision>
RoomAllocator::decode ()
const
{
  std::cout << "total vars: " << var_counter_ << std::endl;
  return {};
}


RoomAllocator::RequestMarker &
RoomAllocator::notify_marker_ (RequestId request_id,
                               RoomId room_id)
{
  if (request_markers_.contains(request_id))
    return request_markers_.at(request_id);
  
  const Request &request = requests_.at(request_id);

  RequestMarker marker;
  marker.room_id = room_id;
  marker.start_date = std::nullopt;
  marker.excluded_dates = std::nullopt;

  for (auto it_room : rooms_) {
    RoomId excl_room_id = it_room.first;
    const Room &excl_room = it_room.second;

    if (excl_room_id == room_id)
      continue;

    if (excl_room.capacity < request.num_people)
      continue;

    marker.excluded_rooms.push_back(excl_room_id);
  }

  unfinished_markers_.insert(request_id);
  return (request_markers_[request_id] = std::move(marker));
}


void
RoomAllocator::notify_assignment (const std::vector<int> &lits)
{
  for (int lit : lits) {
    std::cout << std::format("assigned literal: {} (trail size {})",
      pr_(lit), trail_.size()) << std::endl;
    // std::cout << "assigned " << lit << std::endl;
    trail_.push_back(lit);

    if (vars_top_dec_.contains(lit)) {
      std::cout << "true top: " << pr_(lit) << std::endl;
      true_top_.insert(lit);
      QrTuple qr = vars_top_dec_.at(lit);

      notify_marker_(qr.request_id, qr.room_id);
      continue;
    }
    
    if (vars_norm_dec_.contains(lit)) {
      std::cout << "true norm: " << pr_(lit) << std::endl;
      true_norm_.insert(lit);
      QrdTuple qrd = vars_norm_dec_.at(lit);
      
      RequestMarker &marker =
        notify_marker_(qrd.request_id, qrd.room_id);
      marker.start_date = qrd.date;
      /* XXX: excluded dates? */
      continue;
    }

    if (vars_start_dec_.contains(lit)) {
      std::cout << "true start: " << pr_(lit) << std::endl;
      true_start_.insert(lit);
      QrdTuple qrd = vars_start_dec_.at(lit);

      RequestMarker &marker =
        notify_marker_(qrd.request_id, qrd.room_id);
      marker.start_date = qrd.date;
      /* XXX: exclude dates. */
      continue;
    }
  }
}


void
RoomAllocator::notify_new_decision_level ()
{
  size_t size = trail_.size();
  std::cout << "new decision level: " << size << std::endl;
  level_lims_.push_back(size);
}


void
RoomAllocator::notify_backtrack (size_t new_level)
{
  std::cout << "backtracking down to " << new_level << std::endl;
  
  if (new_level < level_lims_.size()) {
    trail_.resize(level_lims_[new_level]);
    level_lims_.resize(new_level);
  }
}


bool
RoomAllocator::cb_check_found_model (const std::vector<int> &model)
{
  std::cout << "checking found model" << std::endl;
  // true_norm_.clear();
  // true_start_.clear();
  
  /* checking found model */
#if 0
  for (int v : model)
    std::cout << v << std::endl;
#endif

  return true;
}


bool
RoomAllocator::cb_has_external_clause (bool &is_forgettable)
{
  is_forgettable = false;

  if (pending_clauses_.empty())
    return false;

  ext_clause_ = std::move(pending_clauses_.back());
  pending_clauses_.pop_back();
  ext_idx_ = 0;
  return true;
}


int
RoomAllocator::cb_add_external_clause_lit ()
{
  if (ext_idx_ < ext_clause_.size())
    return ext_clause_[ext_idx_++];

  return 0;
}


int
RoomAllocator::cb_propagate ()
{
#if 0
  // std::cout << "cb_propagate yeah yeah" << std::endl;
  if (unfinished_markers_.size() > 0) {
    RequestId request_id = unfinished_markers_.front();
    RequestMarker &marker = request_markers_.at(request_id);

    if (marker.excluded_rooms_idx < marker.excluded_rooms.size()) {
      RoomId room_id =
        marker.excluded_rooms.at(marker.excluded_rooms_idx++);

      QrTuple qr = {request_id, room_id};
      int qrv = vars_top_enc_.at(qr);

      return -qrv;
    }

    /* XXX: check excluded dates */
  }
#endif

  /*
   * Try blocking out whole rooms first
   */
  for (RequestId request_id : unfinished_markers_) {
    RequestMarker &marker = request_markers_.at(request_id);

    if (marker.excluded_rooms_idx < marker.excluded_rooms.size()) {
      RoomId excl_room_id =
        marker.excluded_rooms.at(marker.excluded_rooms_idx++);
      QrTuple prop_qr = {request_id, excl_room_id};
      int prop_lit = -vars_top_enc_.at(prop_qr);
      
      if (marker.is_finished())
        unfinished_markers_.erase(request_id);

      return prop_lit;
    }

    /* XXX: check dates? */
  }

  return 0;
}


int
RoomAllocator::cb_add_reason_clause_lit (int plit)
{
  if (! reason_active_ ) {
    if (plit < 0
     && vars_top_dec_.contains(-plit)) {
      /* QR_{x,y} => -QR_{x, y'} */
      QrTuple qr = vars_top_dec_.at(-plit);
      RequestMarker &marker = request_markers_.at(qr.request_id);

      QrTuple reason_qr = {qr.request_id, marker.room_id};
      int reason_v = vars_top_enc_.at(reason_qr);

      std::cout <<
        std::format("new reason clause: {} => -{}",
          pr_(reason_v), pr_(-plit))
        << std::endl;
      reason_clause_ = {-reason_v, plit};
    }

    reason_idx_ = 0;
    reason_active_ = true;
  }

  if (reason_idx_ < reason_clause_.size())
    return reason_clause_.at(reason_idx_++);

  reason_active_ = false;
  return 0;
}


int
RoomAllocator::cb_decide ()
{
  return 0;
}


/*
 * Private
 */

void
RoomAllocator::register_start_ ()
{
  std::map<RoomId, std::vector<int>> room_var_lists;
  
  for (auto it_request : requests_) {
    RequestId request_id = it_request.first;
    const Request &request = it_request.second;

    std::vector<int> request_top_vars;

    for (auto it_room : rooms_) {
      RoomId room_id = it_room.first;
      const Room &room = it_room.second;

      if (room.capacity < request.num_people)
        continue;

#if 0
      std::cout << std::format("new QR top-level: ({} {})",
        request_id, room_id) << std::endl;
#endif
      QrTuple qr = {request_id, room_id};
      int qrv = new_top_var_(qr);
      request_top_vars.push_back(qrv);

      QrdTuple qrd = {request_id, room_id, 0};
      int qrdsv = maybe_new_start_var_(qrd);
      int qrdv  = maybe_new_norm_var_(qrd);
      room_var_lists[room_id].push_back(qrdv);

      /* QRDSV_{x,y,d} => QRD_{x,y,d} */
      slv_.add(-qrdsv);
      slv_.add(qrdv);
      slv_.add(0);

      /* QRDSV_{x,y,d} => QR_{x,y} */
      slv_.add(-qrdsv);
      slv_.add(qrv);
      slv_.add(0);
    }

    for (int v : request_top_vars)
      slv_.add(v);
    slv_.add(0);
  }

  for (auto it_rv : room_var_lists) {
    const std::vector<int> &room_vars = it_rv.second;

    for (int v : room_vars)
      slv_.add(v);
    slv_.add(0);
  }
}


void
RoomAllocator::flush_observed_ ()
{
  for (int v : pending_observed_)
    slv_.add_observed_var(v);

  pending_observed_.clear();
}


int
RoomAllocator::new_top_var_ (RoomAllocator::QrTuple qr)
{
  int v = ++var_counter_;

  vars_top_enc_[qr] = v;
  vars_top_dec_[v]  = qr;
  pending_observed_.push_back(v);
  
  return v;
}


int
RoomAllocator::maybe_new_norm_var_ (RoomAllocator::QrdTuple qrd)
{
  if (vars_norm_enc_.contains(qrd))
    return vars_norm_enc_.at(qrd);
  
  int v = ++var_counter_;

  vars_norm_enc_[qrd] = v;
  vars_norm_dec_[v]   = qrd;
  pending_observed_.push_back(v);

  return v;
}


int
RoomAllocator::maybe_new_start_var_ (RoomAllocator::QrdTuple qrd)
{
  if (vars_start_enc_.contains(qrd))
    return vars_start_enc_.at(qrd);

  int v = ++var_counter_;

  vars_start_enc_[qrd] = v;
  vars_start_dec_[v]   = qrd;
  pending_observed_.push_back(v);

  return v;
}


/*
 * Debug
 */
std::string
RoomAllocator::pr_ (int v)
const
{
  if (vars_top_dec_.contains(v)) {
    QrTuple qr = vars_top_dec_.at(v);
    return std::format("({} {})", qr.request_id, qr.room_id);
  }

  if (vars_norm_dec_.contains(v)) {
    QrdTuple qrd = vars_norm_dec_.at(v);
    return std::format("({} {} {})",
     qrd.request_id, qrd.room_id, qrd.date);
  }

  if (vars_start_dec_.contains(v)) {
    QrdTuple qrd = vars_start_dec_.at(v);
    return std::format("({} {} {})",
     qrd.request_id, qrd.room_id, qrd.date);
  }

  return "ERROR";
}



static const std::vector<Room> sample_rooms = {
  {1, 20},
  {2, 35},
  {3, 40},
  {4, 16},
};

static const std::vector<Request> sample_requests = {
  {1, 20, 3},
  {2, 20, 3},
  {3, 40, 10},
  {4, 30, 5},
};

static const int sample_days = 10;


int
main ([[maybe_unused]] int argc,
      [[maybe_unused]] char **argv)
{
  RoomAllocator room_allocator(sample_rooms, sample_requests, sample_days);
  int rc = room_allocator.run();
  if (rc == 0) {
    std::vector<Decision> decisions = room_allocator.decode();
    std::cout << "sat" << std::endl;
  }
  return 0;
}
