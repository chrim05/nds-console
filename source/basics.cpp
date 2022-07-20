#include "basics.h"

#include <nds.h>
#include <dirent.h>

void panic(std::string msg)
{
  printf("[!] sys panic `%s`\n", msg.c_str());
  fflush(stdout);

  // keeping opened the process to show the message
  while (true) swiWaitForVBlank();
}

cstring_t cstringRealloc(cstring_t s)
{
  // including the null terminator
  auto temp = new char[strlen(s) + 1];

  return strcpy(temp, s);
}

std::string cutTrailingZeros(std::string s)
{
  return s
    // removing trailing zeros
    .erase(s.find_last_not_of('0') + 1, std::string::npos)
    // removing trailing dot
    .erase(s.find_last_not_of('.') + 1, std::string::npos);
}

std::string getRealPath(std::string path)
{
  auto split  = splitString('/', path);
  auto result = std::vector<std::string>();

  for (auto e : split)
  {
    // skipping `.` useless
    if (e == ".")
      continue;

    // `..` means the last pushed element has to be popped
    if (e == "..")
      result.pop_back();
    // normal element
    else
      result.push_back(e);
  }

  // remerging all paths
  return addTrailingSlashToPath(joinArray<std::string>("/", result, [] (std::string e) { return e; }));
}

std::vector<std::string> splitString(char toSplit, std::string s)
{
  auto result     = std::vector<std::string>();
  auto tempBuffer = std::string();

  for (uint64_t i = 0; i < s.length(); i++)
  {
    auto cur = s[i];

    if (cur == toSplit && !tempBuffer.empty())
    {
      result.push_back(tempBuffer);
      tempBuffer.clear();
    }
    else
      tempBuffer.push_back(cur);
  }

  if (!tempBuffer.empty())
    result.push_back(tempBuffer);

  return result;
}

std::string addTrailingSlashToPath(std::string dir)
{
  if (dir[dir.length() - 1] != '/')
    dir.push_back('/');
  
  return dir;
}

void removeAllInsideDir(std::string path)
{
  auto dir = opendir(path.c_str());

  if (!dir)
    return;

  // iterating the directory
  while (auto entry = readdir(dir))
  {
    if (entry->d_name == std::string(".") || entry->d_name == std::string(".."))
      continue;

    auto fullElemPath = path + '/' + entry->d_name;

    if (entry->d_type == DT_DIR)
      removeAllInsideDir(fullElemPath);
    else
      remove(fullElemPath.c_str());
  }
  
  closedir(dir);
}