# LockLab Memory Ordering Audit

## 1. Treiber Stack

### Push

`head` is loaded with `memory_order_acquire`.

Reason:
- The current head pointer and its linked-node state must be observed before constructing the CAS candidate.

The successful CAS publishes the newly created node with `memory_order_release`.

Reason:
- The initialized node contents must become visible to a thread that subsequently acquires the head.

The failed CAS uses `memory_order_relaxed`.

Reason:
- A failed CAS only provides a newer pointer value; the operation retries and performs the required acquire load on the next iteration.

### Pop

The head is loaded with `memory_order_acquire`.

Reason:
- The thread must observe the node and its initialized `next` pointer.

The hazard pointer is published before dereferencing the protected node.

The head is then rechecked.

Reason:
- This prevents dereferencing a node that was removed between the initial load and hazard publication.

The successful removal CAS must preserve the ordering required by the reclamation protocol.

The hazard pointer is cleared only after the node is no longer accessed.

---

## 2. Michael-Scott Queue

### Enqueue

The queue tail and node-next pointers use acquire/release ordering.

A newly allocated node is fully initialized before its publication through the atomic `next` pointer.

A release operation publishes the initialized node.

A subsequent acquire load observes that initialization.

Helping operations that only move `tail` do not need to publish new application data.

### Dequeue

The current head and the protected successor node are acquired before their fields are accessed.

The successor is protected before dereferencing its value.

The head is revalidated after hazard publication.

The old dummy head is retired only after the successful head transition and after hazard protection is cleared.

---

## 3. Hazard Pointers

Hazard-pointer publication cannot be treated as an ordinary relaxed store.

Required ordering:

1. Load candidate pointer.
2. Publish candidate in hazard slot.
3. Re-read the shared pointer.
4. Only then dereference the candidate.
5. Clear hazard after the final access.
6. Retire/reclaim only when no hazard slot contains the pointer.

The protection publication and validation sequence is therefore deliberately stronger than ordinary data-path operations.

`seq_cst` is currently retained for the hazard publication/scan path until the reclamation protocol is formally reduced and tested.

---

## 4. ABA

The stack/queue reclamation protocol prevents a removed node from being immediately reused while another thread still protects it.

The tagged-pointer demonstration additionally shows that a pointer returning from:

`A -> B -> A`

is not equivalent to the original state because the version changes.

---

## 5. Design Principle

LockLab does not use `memory_order_seq_cst` indiscriminately.

The intended hierarchy is:

- `relaxed`: counters, retry observations, and operations with no synchronization requirement.
- `acquire`: consuming published pointer/data state.
- `release`: publishing initialized nodes/data.
- `acq_rel`: RMW operations that both consume prior state and publish a new state.
- `seq_cst`: retained only where required by the hazard-pointer protocol or demonstration.

Every ordering must have a synchronization or visibility justification.
