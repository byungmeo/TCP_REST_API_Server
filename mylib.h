#ifndef MYLIB_H
#define MYLIB_H

#include <sstream>
#include <vector>

std::vector<std::string> split(std::string input, char delimiter);
std::string& ltrim(std::string& s);
std::string& rtrim(std::string& s);
std::string& trim(std::string& s);

#endif
