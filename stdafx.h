// stdafx.h : include file for standard system include files,
//            or project specific include files that are used
//            frequently, but are changed infrequently

#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cerrno>

#include <unordered_set>
#include <exception>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <map>

#ifdef __linux__ 
#elif   _WIN32
#pragma warning (disable:4996)
#else
#error Platform not supported
#endif

typedef uint32_t        uint_t;
typedef unsigned char  uchar_t;
typedef uint64_t       ulong_t;

