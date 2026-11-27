from asyncio import new_event_loop
import new_enc
import random

MAX_CAPACITY = 1000
DAYS = 365
random.seed(42)
def gen_random_sample(num_req: int, num_room: int):
    new_enc.requests = []
    new_enc.rooms = []
    new_enc._request_id = new_enc._room_id = 0
    for _ in range(num_req):
        num_people = random.randint(1, MAX_CAPACITY)
        length_of_stay = random.randint(1, DAYS)
        new_enc.add_request(num_people, length_of_stay)
    for _ in range(num_room):
        capacity = random.randint(1, MAX_CAPACITY)
        new_enc.add_room(capacity)


if __name__ == "__main__":
    new_enc.days = DAYS
    num_req = int(input("Number of requests: "))
    num_room = int(input("Number of rooms: "))
    gen_random_sample(num_req, num_room)
    slv = new_enc.encode()
    print(slv)
    print("vars:")
    for k, v in new_enc.vars.items():
        print(f"{k}\t-->\t{v}")
    print("request:", new_enc.requests)
    print("room:", new_enc.rooms)
    print("number of clauses:", slv.count(0))
