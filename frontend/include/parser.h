
#ifndef _ONE_IDLC_PARSER_H_
#define _ONE_IDLC_PARSER_H_

#include <memory>
#include <optional>
#include <cstring>

#include "lexer.h"
#include "raw_ast.h"
#include "utils.h"

namespace idlc {


enum class ErrorCode {
  NoError,
  ErrConsumeNotExpected,
  ErrInvalidIdentifier,
  ErrTypeDeclareCompound,
  ErrConstantBody,
  ErrSequenceFormat,
};

class Parser {
 public:
  Parser(Lexer* lexer);

  std::unique_ptr<raw::File> Parse() { return ParseFile(); }

  bool Success() const { return ErrorCode::NoError == error_occured_flag_; }

 private:

  ErrorCode error_occured_flag_;
  // currently the only usecase for this enum is to identify the case where the parser
  // has seen a doc comment block, followed by a regular comment block, followed by
  // a doc comment block
  enum class State {
    // the parser is currently in a doc comment block
    kDocCommentLast,
    // the parser is currently in a regular comment block, which directly followed a
    // doc comment block
    kDocCommentThenComment,
    // the parser is in kNormal for all other cases
    kNormal,
  };


  Token Lex() {
    for (;;) {
      auto token = lexer_->Lex();
      tokens_.emplace_back(std::make_unique<Token>(token));

      switch (token.kind()) {
        case Token::Kind::kComment:
          if (state_ == State::kDocCommentLast)
            state_ = State::kDocCommentThenComment;
          break;
        case Token::Kind::kDocComment:
          if (state_ == State::kDocCommentThenComment)
            ALOGW("WarnCommentWithinDocCommentBlock, DocComment found after comment");
          state_ = State::kDocCommentLast;
          return token;
        default:
          state_ = State::kNormal;
          return token;
      }
    }
  }

  Token::KindAndSubkind Peek() { return last_token_.kind_and_subkind(); }

  // ASTScope is a tool to track the start and end source location of each
  // node automatically.  The parser associates each node with the start and
  // end of its source location.  It also tracks the "gap" in between the
  // start and the previous interesting source element.  As we walk the tree,
  // we create ASTScope objects that can track the beginning and end of the
  // text associated with the Node being built.  The ASTScope object then
  // colludes with the Parser to figure out where the beginning and end of
  // that node are.
  //
  // ASTScope should only be created on the stack, when starting to parse
  // something that will result in a new AST node.
  class ASTScope {
   public:
    explicit ASTScope(Parser* parser) : parser_(parser) {
      suppress_ = parser_->suppress_gap_checks_;
      parser_->suppress_gap_checks_ = false;
      parser_->active_ast_scopes_.push_back(raw::SourceElement(Token(), Token()));
    }
    // The suppress mechanism
    ASTScope(Parser* parser, bool suppress) : parser_(parser), suppress_(suppress) {
      parser_->active_ast_scopes_.push_back(raw::SourceElement(Token(), Token()));
      suppress_ = parser_->suppress_gap_checks_;
      parser_->suppress_gap_checks_ = suppress;
    }
    raw::SourceElement GetSourceElement() {
      parser_->active_ast_scopes_.back().end_ = parser_->previous_token_;
      if (!parser_->suppress_gap_checks_) {
        parser_->last_was_gap_start_ = true;
      }
      return raw::SourceElement(parser_->active_ast_scopes_.back());
    }
    ~ASTScope() {
      parser_->suppress_gap_checks_ = suppress_;
      parser_->active_ast_scopes_.pop_back();
    }

    ASTScope(const ASTScope&) = delete;
    ASTScope& operator=(const ASTScope&) = delete;

   private:
    Parser* parser_;
    bool suppress_;
  };

  void UpdateMarks(Token& token) {
    // There should always be at least one of these - the outermost.
    if (active_ast_scopes_.size() <= 0) {
      ALOGE("internal compiler error: unbalanced parse tree");
    }

    if (!suppress_gap_checks_) {
      // If the end of the last node was the start of a gap, record that.
      if (last_was_gap_start_ && previous_token_.kind() != Token::Kind::kNotAToken) {
        gap_start_ = token.previous_end();
        last_was_gap_start_ = false;
      }

      // If this is a start node, then the end of it will be the start of
      // a gap.
      if (active_ast_scopes_.back().start_.kind() == Token::Kind::kNotAToken) {
        last_was_gap_start_ = true;
      }
    }
    // Update the token to record the correct location of the beginning of
    // the gap prior to it.
    if (gap_start_.valid()) {
      token.set_previous_end(gap_start_);
    }

    for (auto& scope : active_ast_scopes_) {
      if (scope.start_.kind() == Token::Kind::kNotAToken) {
        scope.start_ = token;
      }
    }

    previous_token_ = token;
  }

  bool ConsumedEOF() const { return previous_token_.kind() == Token::Kind::kEndOfFile; }

  template <class Predicate>
  std::optional<Token> ConsumeToken(Predicate p) {
    if (ErrorCode::ErrConsumeNotExpected == p(Peek())) {
      ALOGE("ConsumeToken not match expected token");
      error_occured_flag_ = ErrorCode::ErrConsumeNotExpected;
      return std::nullopt;
    }
    if (ConsumedEOF()) {
      ALOGE("Already consumed EOF");
    }

    auto token = previous_token_ = last_token_;
    // Don't lex any more if we hit EOF. Note: This means that after consuming
    // EOF, Peek() will make it seem as if there's a second EOF.
    if (token.kind() != Token::Kind::kEndOfFile) {
      last_token_ = Lex();
    }
    UpdateMarks(token);
    return token;
  }

  std::unique_ptr<raw::File> ParseFile();

  std::unique_ptr<raw::CompoundIdentifier> ParseModuleNameCompound(ASTScope& scope);

  std::unique_ptr<raw::ConstDeclaration> ParseConstDeclaration(ASTScope& scope);
  std::unique_ptr<raw::StructDeclaration> ParseStructDeclaration(ASTScope& scope);
  std::unique_ptr<raw::UnionDeclaration> ParseUnionDeclaration(ASTScope& scope);
  std::unique_ptr<raw::EnumDeclaration> ParseEnumDeclaration(ASTScope& scope);
  std::unique_ptr<raw::InterfaceDeclaration> ParseInterfaceDeclaration(ASTScope& scope,
    std::unique_ptr<raw::Identifier> attribute);

  std::unique_ptr<raw::Identifier> ParseIdentifier(bool is_discarded = false);

  std::unique_ptr<raw::StringLiteral> ParseStringLiteral();
  std::unique_ptr<raw::NumericLiteral> ParseNumericLiteral();
  std::unique_ptr<raw::TrueLiteral> ParseTrueLiteral();
  std::unique_ptr<raw::FalseLiteral> ParseFalseLiteral();
  std::unique_ptr<raw::Literal> ParseLiteral();

  std::unique_ptr<raw::TypeConstructor> ParseTypeConstructor();
  std::unique_ptr<raw::Constant> ParseConstant();
  bool ParseSequence();

  std::vector<std::unique_ptr<raw::StructMember>> ParseStructMembers();

  std::vector<std::unique_ptr<raw::UnionMember>> ParseUnionMembers();

  std::vector<std::unique_ptr<raw::EnumMember>> ParseEnumMembers();
  bool ParsePointValueInEnumMenber(int& value);

  std::unique_ptr<raw::MethodDeclaration> ParseInterfaceMethod();
  std::unique_ptr<raw::EventDeclaration> ParseInterfaceEvent();
  std::vector<std::unique_ptr<raw::MethodParameter>> ParseInterfaceMethodParam();
  std::vector<std::unique_ptr<raw::EventMember>> ParseInterfaceEventMember();

  static auto IdentifierOfSubkind(Token::Subkind expected_subkind) {
    return [expected_subkind](Token::KindAndSubkind actual) -> ErrorCode {
      auto expected = Token::KindAndSubkind(Token::Kind::kIdentifier, expected_subkind);
      if (actual.combined() != expected.combined()) {
        ALOGE("IdentifierOfSubkind not match, actual combined: %hd, expected combined: %hd", actual.combined(),
          expected.combined());
        return ErrorCode::ErrConsumeNotExpected;
      }
      return ErrorCode::NoError;
    };
  }

  static auto OfKind(Token::Kind expected_kind) {
    return [expected_kind](Token::KindAndSubkind actual) -> ErrorCode {
      if (actual.kind() != expected_kind) {
        ALOGE("OfKind not match, actual kind: %hd, expected kind: %hd", actual.kind(), expected_kind);
        return ErrorCode::ErrConsumeNotExpected;
      }
      return ErrorCode::NoError;
    };
  }


  bool Ok() { return ErrorCode::NoError == error_occured_flag_; }
  void resetErrFlag() { error_occured_flag_ = ErrorCode::NoError; }

  Lexer* lexer_;

  // The stack of information interesting to the currently active ASTScope
  // objects.
  std::vector<raw::SourceElement> active_ast_scopes_;
  // The most recent start of a "gap" - the uninteresting source prior to the
  // beginning of a token (usually mostly containing whitespace).
  SourceSpan gap_start_;
  // Indicates that the last element was the start of a gap, and that the
  // scope should be updated accordingly.
  bool last_was_gap_start_ = false;
  // Suppress updating the gap for the current Scope.  Useful when
  // you don't know whether a scope is going to be interesting lexically, and
  // you have to decide at runtime.
  bool suppress_gap_checks_ = false;
  // The token before last_token_ (below).
  Token previous_token_;

  Token last_token_;
  State state_;

  // An ordered list of all tokens (including comments) in the source file.
  std::vector<std::unique_ptr<Token>> tokens_;
};

}  // namespace idlc

#endif  // _ONE_IDLC_PARSER_H_
