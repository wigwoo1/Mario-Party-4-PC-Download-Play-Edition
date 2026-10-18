#include <stddef.h>
#include <stdint.h>
struct BeforeMusyx { char byte; uint64_t value; };
#include "musyx/sal.h"
#include "musyx/seq.h"
struct AfterMusyx { char byte; uint64_t value; };
_Static_assert(offsetof(struct BeforeMusyx, value) == offsetof(struct AfterMusyx, value),
               "MusyX headers must restore the caller's packing");
_Static_assert(sizeof(struct BeforeMusyx) == sizeof(struct AfterMusyx), "packing leaked");
int main(void) { return 0; }
