
#ifndef _ONE_IDLC_UTILS_H_
#define _ONE_IDLC_UTILS_H_

#include <string>

bool IsValidIdentifierComponent(const std::string& component);

std::string strip_string_literal_quotes(std::string_view str);

#endif  // _ONE_IDLC_UTILS_H_