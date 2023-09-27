#pragma once

#include "rf/config.h"
#include <stdint.h>

C_BEGIN

typedef uint32_t rf_hash32;
typedef rf_hash32 (*rf_hash32_func)(const void*, int);

RF_PUBLIC_API rf_hash32
rf_hash32_jenkins_oaat(const void* key, int len);

RF_PUBLIC_API rf_hash32
rf_hash32_ptr(const void* ptr, int len);

RF_PUBLIC_API rf_hash32
rf_hash32_aligned_ptr(const void* ptr, int len);

/*!
 * @brief Taken from boost::hash_combine. Combines two hash values into a
 * new hash value.
 */
RF_PUBLIC_API rf_hash32
rf_hash32_combine(rf_hash32 lhs, rf_hash32 rhs);

C_END
