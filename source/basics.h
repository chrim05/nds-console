#pragma once

#include <stdio.h>
#include <c++/12.1.0/string>

typedef const char* cstring_t;
typedef char void_t;

inline void panic(std::string msg)
{
  printf("[!] sys panic `%s`\n", msg.c_str());

  // keeping opened the process to show the message
  while(true);
}

inline cstring_t cstringRealloc(cstring_t s)
{
  auto temp = new char[strlen(s) + 1];

  return strcpy(temp, s);
}