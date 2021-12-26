
#include "utils.h"

#include <regex>

#include "log.h"

const std::string kIdentifierComponentPattern = "[A-Za-z]([A-Za-z0-9_]*[A-Za-z0-9])?";

bool IsValidIdentifierComponent(const std::string& component) {
  static const std::regex kPattern("^" + kIdentifierComponentPattern + "$");
  return std::regex_match(component, kPattern);
}

std::string strip_string_literal_quotes(std::string_view str) {
  if (str.size() < 2 || str[0] != '"' || str[str.size() - 1] != '"') {
    ALOGE("string must start and end with '\"' style quotes");
    return std::string("");
  }
  return std::string(str.data() + 1, str.size() - 2);
}