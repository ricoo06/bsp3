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
_var_id = 1
def var(req_id: int, room_id: int, start: int) -> int:
    global vars, _var_id
    t = (req_id, room_id, start)
    if not t in vars:
        vars[t] = _var_id
        _var_id += 1
    return vars[t]

solver = []

def encode():
    global solver
    p = get_feasible_placements()

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
    p = get_feasible_placements()
    for k, v in p.items():
        print("request id", k)
        for r, s in v:
            print("room id", r)
            print("start day", s)
        print()
