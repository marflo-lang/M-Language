#pragma once

#if defined(_MSC_VER)
    // Microsoft Visual Studio
#define M_NORETURN  __declspec(noreturn)
#define M_NOINLINE  __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    // GCC y Clang
#define M_NORETURN  __attribute__((__noreturn__))
#define M_NOINLINE  __attribute__((noinline))
#elif __STDC_VERSION__ >= 201112L
    // C11 Estandar
#define M_NORETURN  _Noreturn
#define M_NOINLINE  
#elif defined(__cplusplus) && __cplusplus >= 201103L
    // C++11 Estandar
#define M_NORETURN  [[noreturn]]
#define M_NOINLINE  
#else
    // Ninguno de los anteriores
#define M_NORETURN  
#define M_NOINLINE  
#endif

