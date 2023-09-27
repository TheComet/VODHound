#include "rf/hash.h"
#include <assert.h>

/* ------------------------------------------------------------------------- */
rf_hash32
rf_hash32_jenkins_oaat(const void* key, int len)
{
    rf_hash32 hash = 0;
    int i = 0;
    for(; i != len; ++i)
    {
        hash += *((uint8_t*)key + i);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 1);
    hash += (hash << 15);
    return hash;
}

/* ------------------------------------------------------------------------- */
#if RF_SIZEOF_VOID_P == 8
rf_hash32
rf_hash32_ptr(const void* ptr, int len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return rf_hash32_combine(
           (rf_hash32)(*(uintptr_t*)ptr & 0xFFFFFFFF),
           (rf_hash32)(*(uintptr_t*)ptr >> 32)
    );
}
#elif RF_SIZEOF_VOID_P == 4
rf_hash32
rf_hash32_ptr(const void* ptr, uintptr_t len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return (rf_hash32)*(uintptr_t*)ptr;
}
#endif

/* ------------------------------------------------------------------------- */
#if RF_SIZEOF_VOID_P == 8
rf_hash32
rf_hash32_aligned_ptr(const void* ptr, int len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return (rf_hash32)((*(uintptr_t*)ptr / sizeof(void*)) & 0xFFFFFFFF);
}
#elif RF_SIZEOF_VOID_P == 4
rf_hash32
rf_hash32_aligned_ptr(const void* ptr, uintptr_t len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return (rf_hash32)(*(uintptr_t*)ptr / sizeof(void*));
}
#endif

/* ------------------------------------------------------------------------- */
rf_hash32
rf_hash32_combine(rf_hash32 lhs, rf_hash32 rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}
