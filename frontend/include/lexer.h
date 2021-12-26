
#ifndef _ONE_IDLC_FIDL_LEXER_H_
#define _ONE_IDLC_FIDL_LEXER_H_

#include <assert.h>
#include <stdint.h>

#include <map>
#include <string_view>

#include "source_manager.h"
#include "token.h"
#include "log.h"

namespace idlc {

// The lexer does not own the data it operates on. It merely takes a
// std::string_view and produces a stream of tokens and possibly a failure
// partway through.
class Lexer {
 public:
  // The Lexer assumes the final character is 0. This substantially
  // simplifies advancing to the next character.
  Lexer(const SourceFile& source_file)
      : source_file_(source_file) {
    token_subkinds = {
#define TOKEN_SUBKIND(Name, Spelling) {Spelling, Token::Subkind::k##Name},
#include "token_definitions.inc"
#undef TOKEN_SUBKIND
    };
    current_ = data().data();
    end_of_file_ = current_ + data().size();
    previous_end_ = token_start_ = current_;
  }

  // Lexes and returns the next token. Must not be called again after returning
  // Token::Kind::kEndOfFile.
  Token Lex();

 private:
  std::string_view data() { return source_file_.data(); }

  constexpr char Peek() const;
  void Skip();
  char Consume();
  std::string_view Reset(Token::Kind kind);
  Token Finish(Token::Kind kind);

  void SkipWhitespace();

  Token LexEndOfStream();
  Token LexNumericLiteral();
  Token LexIdentifier();
  Token LexStringLiteral();
  Token LexCommentOrDocComment();
  Token LexBlockComment();

  const SourceFile& source_file_;
  std::map<std::string_view, Token::Subkind> token_subkinds;

  const char* current_ = nullptr;
  const char* end_of_file_ = nullptr;
  const char* token_start_ = nullptr;
  const char* previous_end_ = nullptr;
  size_t token_size_ = 0u;
};

}  // namespace idlc

#endif  // _ONE_IDLC_FIDL_LEXER_H_
