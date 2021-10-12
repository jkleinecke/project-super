#pragma once
#ifndef PROJECT_SUPER_H_
#define PROJECT_SUPER_H_

// Main header file for the game and platform interface

// Standard includes and types
#include <stdint.h>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float   real32;
typedef double  real64;

typedef int32 bool32;

// Standard Defines

// Clarify usage of static keyword with these defines
#define internal static
#define local_persist static
#define global_variable static


#endif