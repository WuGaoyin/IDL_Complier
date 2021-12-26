
#ifndef _ONE_IDLC_RAW_AST_H_
#define _ONE_IDLC_RAW_AST_H_

#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <deque>

#include "source_span.h"
#include "token.h"
#include "types.h"
#include "utils.h"

// ASTs fresh out of the oven. This is a tree-shaped bunch of nodes
// pretty much exactly corresponding to the grammar of a single idl
// file. File is the root of the tree, and consists of lists of
// Declarations, and so on down to individual SourceSpans.

// Each node owns its children via unique_ptr and vector. All tokens
// here, like everywhere in the idl compiler, are backed by a string
// view whose contents are owned by a SourceManager.

// This class has a tight coupling with the TreeVisitor class.  Each node has a
// corresponding method in that class.  Each node type also has an Accept()
// method to help visitors visit the node.  When you add a new node, or add a
// field to an existing node, you must ensure the Accept method works.

// A raw::File is produced by parsing a token stream. All of the
// Files in a library are then flattened out into a Library.

namespace idlc {
namespace raw {

// In order to be able to associate AST nodes with their original source, each
// node is a SourceElement, which contains information about the original
// source.  The AST has a start token, whose previous_end field points to the
// end of the previous AST node, and an end token, which points to the end of
// this syntactic element.
//
// Note: The file may have a tail of whitespace / comment text not explicitly
// associated with any node.  In order to reconstruct that text, raw::File
// contains an end token; the previous_end field of that token points to the end
// of the last interesting token.
class TreeVisitor;

class SourceElement {
 public:
  enum Kind {
    kConst,
    kStruct,
    kUnion,
    kEnum,
    kInterface,
    kComponent,
  };

  explicit SourceElement(SourceElement const& element)
      : start_(element.start_), end_(element.end_), kind_(Kind::kComponent) {}

  explicit SourceElement(SourceElement const& element, Kind kind)
      : start_(element.start_), end_(element.end_), kind_(kind) {}

  explicit SourceElement(Token start, Token end) : start_(start), end_(end), kind_(Kind::kComponent) {}

  explicit SourceElement(Token start, Token end, Kind kind) : start_(start), end_(end), kind_(kind) {}

  bool has_span() const {
    return start_.span().valid() && end_.span().valid() &&
           &start_.span().source_file() == &end_.span().source_file();
  }

  SourceSpan span() const {
    if (!start_.span().valid() || !end_.span().valid()) {
      return SourceSpan();
    }

    assert(has_span());
    const char* start_pos = start_.span().data().data();
    const char* end_pos = end_.span().data().data() + end_.span().data().length();
    return SourceSpan(std::string_view(start_pos, end_pos - start_pos),
                      start_.span().source_file());
  }

  std::string copy_to_str() const {
    const char* start_pos = start_.span().data().data();
    const char* end_pos = end_.span().data().data() + end_.span().data().length();
    return std::string(start_pos, end_pos);
  }

  void update_span(SourceElement const& element) {
    start_ = element.start_;
    end_ = element.end_;
  }

  virtual ~SourceElement() {}

  Token start_;
  Token end_;
  Kind kind_;
};

class SourceElementMark {
 public:
  SourceElementMark(TreeVisitor* tv, const SourceElement& element);

  ~SourceElementMark();

 private:
  TreeVisitor* tv_;
  const SourceElement& element_;
};

class Literal : public SourceElement {
 public:
  enum struct Kind {
    kDocComment,
    kString,
    kNumeric,
    kTrue,
    kFalse,
  };

  explicit Literal(SourceElement const& element, Kind kind) : SourceElement(element), kind(kind) {}

  virtual ~Literal() {}

  const Kind kind;
};

class StringLiteral final : public Literal {
 public:
  explicit StringLiteral(SourceElement const& element) : Literal(element, Kind::kString) {}

  void Accept(TreeVisitor* visitor) const;

  std::string MakeContents() const {
    if (!has_span() || span().data().empty()) {
      return "";
    }
    return strip_string_literal_quotes(span().data());
  }
};

class NumericLiteral final : public Literal {
 public:
  NumericLiteral(SourceElement const& element) : Literal(element, Kind::kNumeric) {}

  void Accept(TreeVisitor* visitor) const;
};

class TrueLiteral final : public Literal {
 public:
  TrueLiteral(SourceElement const& element) : Literal(element, Kind::kTrue) {}

  void Accept(TreeVisitor* visitor) const;
};

class FalseLiteral final : public Literal {
 public:
  FalseLiteral(SourceElement const& element) : Literal(element, Kind::kFalse) {}

  void Accept(TreeVisitor* visitor) const;
};

class Identifier final : public SourceElement {
 public:
  explicit Identifier(SourceElement const& element)
     : SourceElement(element) {}

  void Accept(TreeVisitor* visitor) const;
};

class CompoundIdentifier final : public SourceElement {
 public:
  CompoundIdentifier(SourceElement const& element,
                     std::vector<std::unique_ptr<Identifier>> components)
      : SourceElement(element), components(std::move(components)) {}

  void Accept(TreeVisitor* visitor) const;

  std::vector<std::unique_ptr<Identifier>> components;
};

class TypeConstructor final : public SourceElement {
 public:
  TypeConstructor(SourceElement const& element,
                     std::vector<std::unique_ptr<Identifier>> components,
                     std::deque<int> sequence_size_recorder)
      : SourceElement(element), sequence_size_recorder(sequence_size_recorder), components(std::move(components)) {}

  void Accept(TreeVisitor* visitor) const;

  std::vector<std::unique_ptr<Identifier>> components;
  std::deque<int> sequence_size_recorder;
};

/*********************************** ConstDeclaration start *********************************************/
class Constant : public SourceElement {
 public:
  enum class Kind { kLiteral };

  explicit Constant(Token token, Kind kind) : SourceElement(token, token), kind(kind) {}
  explicit Constant(const SourceElement& element, Kind kind) : SourceElement(element), kind(kind) {}

  virtual ~Constant() {}

  const Kind kind;
};

class LiteralConstant final : public Constant {
 public:
  explicit LiteralConstant(std::unique_ptr<Literal> literal)
      : Constant(literal->start_, Kind::kLiteral), literal(std::move(literal)) {}

  std::unique_ptr<Literal> literal;

  void Accept(TreeVisitor* visitor) const;
};

class ConstDeclaration final : public SourceElement {
 public:
  ConstDeclaration(SourceElement const& element,
                   std::unique_ptr<TypeConstructor> type,
                   std::unique_ptr<Identifier> name, std::unique_ptr<Constant> constant)
      : SourceElement(element, Kind::kConst),
        type(std::move(type)),
        name(std::move(name)),
        constant(std::move(constant)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<TypeConstructor> type;
  std::unique_ptr<Identifier> name;
  std::unique_ptr<Constant> constant;
};
/*********************************** ConstDeclaration end  *********************************************/

/********************************* StructDeclaration start  ********************************************/
class StructMember final : public SourceElement {
 public:
  StructMember(SourceElement const& element, std::unique_ptr<TypeConstructor> type,
                  std::unique_ptr<Identifier> name)
      : SourceElement(element),
        type(std::move(type)),
        name(std::move(name)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<TypeConstructor> type;
  std::unique_ptr<Identifier> name;
};

class StructDeclaration final : public SourceElement {
 public:
  StructDeclaration(SourceElement const& element, std::unique_ptr<Identifier> name,
                        std::vector<std::unique_ptr<StructMember>> members)
      : SourceElement(element, Kind::kStruct),
        name(std::move(name)),
        members(std::move(members)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::vector<std::unique_ptr<StructMember>> members;
};
/********************************** StructDeclaration end  *********************************************/

/********************************** UnionDeclaration start  ********************************************/
class UnionMember final : public SourceElement {
 public:
  UnionMember(SourceElement const& element, std::unique_ptr<TypeConstructor> type,
                  std::unique_ptr<Identifier> name, std::unique_ptr<NumericLiteral> case_value)
      : SourceElement(element),
        type(std::move(type)),
        name(std::move(name)),
        case_value(std::move(case_value)),
        is_default_member(false) {}

  UnionMember(SourceElement const& element, std::unique_ptr<TypeConstructor> type,
                  std::unique_ptr<Identifier> name)
      : SourceElement(element),
        type(std::move(type)),
        name(std::move(name)),
        is_default_member(true) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<TypeConstructor> type;
  std::unique_ptr<Identifier> name;
  std::unique_ptr<NumericLiteral> case_value;
  bool is_default_member;
};

class UnionDeclaration final : public SourceElement {
 public:
  UnionDeclaration(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::vector<std::unique_ptr<UnionMember>> members, std::unique_ptr<TypeConstructor> select_type)
      : SourceElement(element, Kind::kUnion),
        name(std::move(name)),
        members(std::move(members)),
        select_type(std::move(select_type)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::vector<std::unique_ptr<UnionMember>> members;
  std::unique_ptr<TypeConstructor> select_type;
};
/*********************************** UnionDeclaration end  *********************************************/

/*********************************** EnumDeclaration start *********************************************/
class EnumMember final : public SourceElement {
 public:
  EnumMember(SourceElement const& element, std::unique_ptr<Identifier> name, int value)
      : SourceElement(element),
        name(std::move(name)),
        value(value) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  int value;
};

class EnumDeclaration final : public SourceElement {
 public:
  EnumDeclaration(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::vector<std::unique_ptr<EnumMember>> members)
      : SourceElement(element, Kind::kEnum),
        name(std::move(name)),
        members(std::move(members)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::vector<std::unique_ptr<EnumMember>> members;
};
/***********************************  EnumDeclaration end  *********************************************/

/*******************************  InterfaceDeclaration start  ******************************************/
class MethodParameter final : public SourceElement {
 public:
  MethodParameter(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::unique_ptr<TypeConstructor> type)
      : SourceElement(element),
        name(std::move(name)),
        type(std::move(type)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::unique_ptr<TypeConstructor> type;
};

class MethodReturn final : public SourceElement {
 public:
  MethodReturn(SourceElement const& element, std::unique_ptr<TypeConstructor> type)
      : SourceElement(element),
        type(std::move(type)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<TypeConstructor> type;
};

class MethodDeclaration final : public SourceElement {
 public:
  MethodDeclaration(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::vector<std::unique_ptr<MethodReturn>> returns,
                    std::vector<std::unique_ptr<MethodParameter>> parameters)
      : SourceElement(element),
        name(std::move(name)),
        returns(std::move(returns)),
        parameters(std::move(parameters)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::vector<std::unique_ptr<MethodReturn>> returns;
  std::vector<std::unique_ptr<MethodParameter>> parameters;
};

class EventMember final : public SourceElement {
 public:
  EventMember(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::unique_ptr<Identifier> attribute,
                    std::unique_ptr<TypeConstructor> type)
      : SourceElement(element),
        name(std::move(name)),
        attribute(std::move(attribute)),
        type(std::move(type)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::unique_ptr<Identifier> attribute;
  std::unique_ptr<TypeConstructor> type;
};

class EventDeclaration final : public SourceElement {
 public:
  EventDeclaration(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::vector<std::unique_ptr<EventMember>> members)
      : SourceElement(element),
        name(std::move(name)),
        members(std::move(members)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::vector<std::unique_ptr<EventMember>> members;
};

class InterfaceDeclaration final : public SourceElement {
 public:
  InterfaceDeclaration(SourceElement const& element, std::unique_ptr<Identifier> name,
                    std::unique_ptr<Identifier> attribute,
                    std::vector<std::unique_ptr<MethodDeclaration>> methods,
                    std::vector<std::unique_ptr<EventDeclaration>> events)
      : SourceElement(element, Kind::kInterface),
        name(std::move(name)),
        attribute(std::move(attribute)),
        methods(std::move(methods)),
        events(std::move(events)) {}

  void Accept(TreeVisitor* visitor) const;

  std::unique_ptr<Identifier> name;
  std::unique_ptr<Identifier> attribute;
  std::vector<std::unique_ptr<MethodDeclaration>> methods;
  std::vector<std::unique_ptr<EventDeclaration>> events;
};
/********************************  InterfaceDeclaration end  *******************************************/

class File final : public SourceElement {
 public:
  File(SourceElement const& element, Token end, std::unique_ptr<CompoundIdentifier> module_name,
       std::vector<std::unique_ptr<ConstDeclaration>> const_declaration_list,
       std::vector<std::unique_ptr<StructDeclaration>> struct_declaration_list,
       std::vector<std::unique_ptr<UnionDeclaration>> union_declaration_list,
       std::vector<std::unique_ptr<EnumDeclaration>> enum_declaration_list,
       std::vector<std::unique_ptr<InterfaceDeclaration>> interface_declaration_list,
       std::vector<std::unique_ptr<Token>> tokens)
      : SourceElement(element),
        module_name(std::move(module_name)),
        const_declaration_list(std::move(const_declaration_list)),
        struct_declaration_list(std::move(struct_declaration_list)),
        union_declaration_list(std::move(union_declaration_list)),
        enum_declaration_list(std::move(enum_declaration_list)),
        interface_declaration_list(std::move(interface_declaration_list)),
        tokens(std::move(tokens)),
        end_(end) {}

  void Accept(TreeVisitor* visitor) const;
  std::unique_ptr<CompoundIdentifier> module_name;
  std::vector<std::unique_ptr<ConstDeclaration>> const_declaration_list;
  std::vector<std::unique_ptr<StructDeclaration>> struct_declaration_list;
  std::vector<std::unique_ptr<UnionDeclaration>> union_declaration_list;
  std::vector<std::unique_ptr<EnumDeclaration>> enum_declaration_list;
  std::vector<std::unique_ptr<InterfaceDeclaration>> interface_declaration_list;

  // An ordered list of all tokens (including comments) in the source file.
  std::vector<std::unique_ptr<Token>> tokens;
  Token end_;
};

}  // namespace raw
}  // namespace idlc

#endif  // _ONE_IDLC_RAW_AST_H_
