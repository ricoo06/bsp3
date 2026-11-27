requests: list[tuple[int, int, int]] = []
_request_id: int = 0
def add_request(num_people: int, length_of_stay: int) -> None:
    global requests, _request_id
    requests.append((_request_id, num_people, length_of_stay))
    _request_id += 1

rooms: list[tuple[int, int]] = []
_room_id: int = 0
def add_room(capacity: int) -> None:
    global rooms, _room_id
    rooms.append((_room_id, capacity))
    _room_id += 1

days: int = 10

def get_feasible_placements() -> dict[int, list[tuple[int, int]]]:
    """returns feasible placements in the form:
       req_id -> [(room_id, start_day)]
    """
    global rooms, requests, days
    p: dict[int, list[tuple[int, int]]] = dict()
    for req in requests:
        p[req[0]] = []
        for room in rooms:
            if req[1] <= room[1]:
                # room has a big enough capacity
                for s in range(days):
                    if s + req[2] - 1 < days:
                        p[req[0]].append((room[0], s))
    return p

vars: dict[tuple[int, int, int], int] = dict()
_vars: dict[int, tuple[int, int, int]] = dict()
_var_id = 1
def var(req_id: int, room_id: int, start: int) -> int:
    global vars, _var_id
    t = (req_id, room_id, start)
    if not t in vars:
        vars[t] = _var_id
        _vars[_var_id] = t
        _var_id += 1
    return vars[t]

def encode() -> list[int]:
    solver: list[int] = []
    p = get_feasible_placements()
    p_items = list(p.items())
    get_length_of_stay = lambda req_id: requests[req_id][2]
    for req_id, p_i in p_items:
        for room_id, start in p_i:
            # at least one booking per request
            solver.append(var(req_id, room_id, start))
        solver.append(0)
        for i, (room_id_a, start_a) in enumerate(p_i[:-1]):
            # at most one booking per request
            for room_id_b, start_b in p_i[i + 1:]:
                solver.append(-var(req_id, room_id_a, start_a))
                solver.append(-var(req_id, room_id_b, start_b))
                solver.append(0)
    for i, (req_id_a, p_a) in enumerate(p_items[:-1]):
        for req_id_b, p_b in p_items[i + 1:]:
            for room_id_a, start_a in p_a:
                for room_id_b, start_b in p_b:
                    if room_id_a == room_id_b:
                        length_a = get_length_of_stay(req_id_a)
                        length_b = get_length_of_stay(req_id_b)
                        if start_a <= start_b + length_b - 1 and start_b <= start_a + length_a - 1:
                            # forbidding overlapping bookings
                            solver.append(-var(req_id_a, room_id_a, start_a))
                            solver.append(-var(req_id_b, room_id_b, start_b))
                            solver.append(0)
    return solver

if __name__ == "__main__":
    add_room(20)
    add_room(35)
    add_room(40)
    add_room(16)
    add_request(20, 3)
    add_request(20, 3)
    add_request(40, 10)
    add_request(30, 5)
    print("requests: ", requests)
    print("rooms: ", rooms)
    # p = get_feasible_placements()
    # for k, v in p.items():
    #     print("request id", k)
    #     for r, s in v:
    #         print("room id", r)
    #         print("start day", s)
    #     print()
    slv = encode()
    print(slv)
    print(vars)
    print("number of clauses:", slv.count(0))
