#pragma once

#include <stdbool.h>
#include "kissc/hashtable.h"

struct rcorder_options_t {
    bool reverse;
    HashTable ks;
    size_t keep_count;
    bool include_orphans;
};
