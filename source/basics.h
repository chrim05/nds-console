#pragma once

#include <stdio.h>
#include <c++/12.1.0/vector>
#include <c++/12.1.0/functional>
#include <c++/12.1.0/algorithm>
#include <c++/12.1.0/string>

typedef const char* cstring_t;
typedef char void_t;

void panic(std::string msg);

// NOTE: `s` won't be freed
// in this project cstringRealloc is called on .c_str() to have the guarantee that the pointer will not be implicitly deallocated in any case.
// from https://codeql.github.com/codeql-query-help/cpp/cpp-return-c-str-of-std-string/
// ```
//  The pointer is only safe to use while the std::string is still in scope.
//  When the std::string goes out of scope, its destructor is called and the memory is deallocated, so it is no longer safe to use the pointer.
// ```
cstring_t cstringRealloc(cstring_t s);

std::string cutTrailingZeros(std::string s);

// simplifies the paths, examples:
//  `/foo/bar/../` -> `/foo/`
//  `/foo/./bar/.` -> `/foo/bar/`
//  `/foo//bar/`   -> `/foo/bar/`
std::string getRealPath(std::string path);

std::vector<std::string> splitString(char toSplit, std::string s);

template <typename T> std::string joinArray(std::string sep, std::vector<T> arr, std::function<std::string(T)> toStringRemapper)
{
  auto result = std::string();

  for (uint64_t i = 0; i < arr.size(); i++)
  {
    // when this is not the first element
    if (i > 0)
      result.append(sep);

    result.append(toStringRemapper(arr[i]));
  }

  return result;
}

std::string addTrailingSlashToPath(std::string dir);

void removeAllInsideDir(std::string path);

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