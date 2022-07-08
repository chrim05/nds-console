#pragma once

#include <stdio.h>
#include <c++/12.1.0/string>

typedef const char* cstring;

void panic(std::string msg)
{
  printf("[!] sys panic `%s`\n", msg.c_str());

  // keeping opened the process to show the message
  while(true);
}