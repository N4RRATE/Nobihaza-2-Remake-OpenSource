#pragma once
#include <string>
#include <string_view>
#include <iosfwd>
// Package integrity only; no passwords or security credentials use this helper.
std::string contentSha256(std::string_view bytes);
std::string readContentBytes(std::istream& input);
