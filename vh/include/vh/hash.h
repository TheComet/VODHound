#pragma once

#include "vh/config.h"
#include <stdint.h>

C_BEGIN

typedef uint32_t hash32;
typedef hash32 (*hash32_func)(const void*, int);

VH_PUBLIC_API hash32
hash32_jenkins_oaat(const void* key, int len);

VH_PUBLIC_API hash32
hash32_ptr(const void* ptr, int len);

VH_PUBLIC_API hash32
hash32_aligned_ptr(const void* ptr, int len);

/*!
 * @brief Taken from boost::hash_combine. Combines two hash values into a
 * new hash value.
 */
VH_PUBLIC_API hash32
hash32_combine(hash32 lhs, hash32 rhs);

C_END
