module;

export module glox;

import std;
import std.compat;

#define GLOX_EXPORT export
#define GLOX_BEGIN_EXPORT export {
#define GLOX_END_EXPORT }
#define GLOX_MODULE_BUILD
#define GLOX_COMPILING_MODULE

// clang-format off
#include "glox/util.hpp"
#include "glox/utilalgs.hpp"
#include "glox/detail/moveutils.hpp"
#include "glox/detail/memory.hpp"
#include "glox/hash.hpp"
#include "glox/detail/movesem.hpp"
#include "glox/string.h"
#include "glox/metaprog.hpp"
#include "glox/option.hpp"
#include "glox/result.hpp"
#include "glox/algo.hpp"
#include "glox/alloc_types.hpp"
#include "glox/alloc.hpp"
#include "glox/array.hpp"
#include "glox/assert.hpp"
#include "glox/bitfields.hpp"
#include "glox/box.hpp"
#include "glox/intrusive.hpp"
#include "glox/intrusive_fwd_list.hpp"
#include "glox/intrusive_list.hpp"
#include "glox/intrusive_rb_tree.hpp"
#include "glox/legacy_linkedlist.hpp"
#include "glox/math.hpp"
#include "glox/mutex.hpp"
#include "glox/tuple.hpp"
#include "glox/types.hpp"
#include "glox/variant.hpp"
#include "glox/vector.hpp"
#include "glox/string.hpp"
#include "glox/format.hpp"
#include "glox/logger.hpp"
#include "glox/hash_map.hpp"
