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

// in this project cstringRealloc is called on .c_str() to have the guarantee that the pointer will not be implicitly deallocated in any case.
// from https://codeql.github.com/codeql-query-help/cpp/cpp-return-c-str-of-std-string/
// ```
//  The pointer is only safe to use while the std::string is still in scope.
//  When the std::string goes out of scope, its destructor is called and the memory is deallocated, so it is no longer safe to use the pointer.
// ```
inline cstring_t cstringRealloc(cstring_t s)
{
  // including the null terminator
  auto temp = new char[strlen(s) + 1];

  return strcpy(temp, s);
}

inline std::string cutTrailingZeros(std::string s)
{
  return s
    // removing trailing zeros
    .erase(s.find_last_not_of('0') + 1, std::string::npos)
    // removing trailing dot
    .erase(s.find_last_not_of('.') + 1, std::string::npos);
}

template<typename Tk, typename Tv> class KeyPair
{
  public: Tk key;
  public: Tv val;

  public: KeyPair(Tk key, Tv val)
  {
    this->key = key;
    this->val = val;
  }
};