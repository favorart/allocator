// stdafx.h : include file for standard system include files,
//            or project specific include files that are used
//            frequently, but are changed infrequently

#pragma once

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#pragma warning (disable:4996)
#endif

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <exception>
#include <iostream>
#include <vector>

typedef uint32_t        uint_t;
typedef uint64_t       ulong_t;
typedef unsigned char  uchar_t;
