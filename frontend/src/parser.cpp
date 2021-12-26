
#include "parser.h"

#include <errno.h>
#include <functional>

#include "log.h"

namespace idlc {

// The "case" keyword is not folded into CASE_TOKEN and CASE_IDENTIFIER because
// doing so confuses clang-format.
#define CASE_TOKEN(K) Token::KindAndSubkind(K, Token::Subkind::kNone).combined()

#define CASE_IDENTIFIER(K) Token::KindAndSubkind(Token::Kind::kIdentifier, K).combined()

#define TOKEN_TYPE_CASES                         \
  case CASE_IDENTIFIER(Token::Subkind::kNone):   \
  case CASE_IDENTIFIER(Token::Subkind::kArray):  \
  case CASE_IDENTIFIER(Token::Subkind::kVector): \
  case CASE_IDENTIFIER(Token::Subkind::kString): \
  case CASE_IDENTIFIER(Token::Subkind::kRequest)

#define TOKEN_ATTR_CASES         \
  case Token::Kind::kDocComment: \
  case Token::Kind::kLeftSquare

#define TOKEN_LITERAL_CASES                      \
  case CASE_IDENTIFIER(Token::Subkind::kTrue):   \
  case CASE_IDENTIFIER(Token::Subkind::kFalse):  \
  case CASE_TOKEN(Token::Kind::kNumericLiteral): \
  case CASE_TOKEN(Token::Kind::kStringLiteral)

#define ASSERT_OK_OR_FAIL(LOG, RETURN) \
  if (!Ok()) {                         \
      ALOGE(LOG);                      \
      return RETURN;                   \
  }

namespace {

enum {
  More,
  Done,
};

template <typename T, typename Fn>
void add(std::vector<std::unique_ptr<T>>* elements, Fn producer_fn) {
  std::function<std::unique_ptr<T>()> producer(producer_fn);
  auto element = producer();
  if (element)
    elements->emplace_back(std::move(element));
}

}  // namespace

Parser::Parser(Lexer* lexer)
    : lexer_(lexer),
      state_(State::kNormal) {
  error_occured_flag_ = ErrorCode::NoError;
  last_token_ = Lex();
}

std::unique_ptr<raw::StringLiteral> Parser::ParseStringLiteral() {
  ASTScope scope(this);
  ConsumeToken(OfKind(Token::Kind::kStringLiteral));
  ASSERT_OK_OR_FAIL("error occured when consume stringLiteral in Parser::ParseStringLiteral", nullptr);

  return std::make_unique<raw::StringLiteral>(scope.GetSourceElement());
}

std::unique_ptr<raw::NumericLiteral> Parser::ParseNumericLiteral() {
  ASTScope scope(this);
  ConsumeToken(OfKind(Token::Kind::kNumericLiteral));
  ASSERT_OK_OR_FAIL("error occured when consume numericLiteral in Parser::ParseNumericLiteral", nullptr);

  return std::make_unique<raw::NumericLiteral>(scope.GetSourceElement());
}

std::unique_ptr<raw::TrueLiteral> Parser::ParseTrueLiteral() {
  ASTScope scope(this);
  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kTrue));
  ASSERT_OK_OR_FAIL("error occured when consume true in Parser::ParseTrueLiteral", nullptr);

  return std::make_unique<raw::TrueLiteral>(scope.GetSourceElement());
}

std::unique_ptr<raw::FalseLiteral> Parser::ParseFalseLiteral() {
  ASTScope scope(this);
  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kFalse));
  ASSERT_OK_OR_FAIL("error occured when consume false in Parser::ParseFalseLiteral", nullptr);

  return std::make_unique<raw::FalseLiteral>(scope.GetSourceElement());
}

std::unique_ptr<raw::Literal> Parser::ParseLiteral() {
  switch (Peek().combined()) {
    case CASE_TOKEN(Token::Kind::kStringLiteral):
      return ParseStringLiteral();

    case CASE_TOKEN(Token::Kind::kNumericLiteral):
      return ParseNumericLiteral();

    case CASE_IDENTIFIER(Token::Subkind::kTrue):
      return ParseTrueLiteral();

    case CASE_IDENTIFIER(Token::Subkind::kFalse):
      return ParseFalseLiteral();

    default:
      return nullptr;
  }
}

std::unique_ptr<raw::Constant> Parser::ParseConstant() {
  std::unique_ptr<raw::Constant> constant;

  switch (Peek().combined()) {
    TOKEN_LITERAL_CASES : {
      auto literal = ParseLiteral();
      ASSERT_OK_OR_FAIL("error occured when parse literal in Parser::ParseConstant", nullptr);
      constant = std::make_unique<raw::LiteralConstant>(std::move(literal));
      break;
    }

    default: {
      error_occured_flag_ = ErrorCode::ErrConstantBody;
      return nullptr;
    }
  }
  return constant;
}

std::unique_ptr<raw::Identifier> Parser::ParseIdentifier(bool is_discarded) {
  ASTScope scope(this, is_discarded);
  std::optional<Token> token = ConsumeToken(OfKind(Token::Kind::kIdentifier));
  if (!Ok() || !token)
    return nullptr;
  if (!IsValidIdentifierComponent(std::string(token->data()))) {
    ALOGD("invalid identifier component declare");
    error_occured_flag_= ErrorCode::ErrInvalidIdentifier;
    return nullptr;
  }

  return std::make_unique<raw::Identifier>(scope.GetSourceElement());
}

std::unique_ptr<raw::TypeConstructor> Parser::ParseTypeConstructor() {
  ASTScope scope(this);
  std::vector<std::unique_ptr<raw::Identifier>> components;
  std::deque<int> sequnce_size_recorder;

  auto parse_data_single_type = [&]() -> bool {
    if (CASE_IDENTIFIER(Token::Subkind::kUnsigned) == Peek().combined()) {
      components.emplace_back(ParseIdentifier());
      if (CASE_IDENTIFIER(Token::Subkind::kLong) == Peek().combined()) {
        components.emplace_back(ParseIdentifier());
        if (CASE_IDENTIFIER(Token::Subkind::kLong) == Peek().combined()) {
          components.emplace_back(ParseIdentifier());
        }
      } else if (CASE_IDENTIFIER(Token::Subkind::kShort) == Peek().combined()) {
        components.emplace_back(ParseIdentifier());
      } else {
        ALOGE("Error token(combined: %hd) exist after Unsigned", Peek().combined());
        error_occured_flag_ = ErrorCode::ErrTypeDeclareCompound;
        return false;
      }
    } else if (CASE_IDENTIFIER(Token::Subkind::kLong) == Peek().combined()) {
      components.emplace_back(ParseIdentifier());
      if (CASE_IDENTIFIER(Token::Subkind::kLong) == Peek().combined()) {
        components.emplace_back(ParseIdentifier());
      }
    } else {
      components.emplace_back(ParseIdentifier());
    }
    return true;
  };

  auto sequence_count_left  = 0;
  auto sequence_count_right = 0;
  while(CASE_IDENTIFIER(Token::Subkind::kSequence) == Peek().combined()) {
    ConsumeToken(IdentifierOfSubkind(Token::Subkind::kSequence));
    ASSERT_OK_OR_FAIL("error occured when consume sequence in Parser::ParseTypeConstructor", nullptr);
    ConsumeToken(OfKind(Token::Kind::kLeftAngle));
    ASSERT_OK_OR_FAIL("error occured when consume leftAngle in Parser::ParseTypeConstructor", nullptr);
    ++sequence_count_left;
  }

  if (!parse_data_single_type()) {
    ALOGE("parse_data_single_type error in Parser::ParseTypeConstructor");
    return nullptr;
  }

  while (CASE_TOKEN(Token::Kind::kComma) == Peek().combined() || CASE_TOKEN(Token::Kind::kRightAngle) == Peek().combined()) {
    if (CASE_TOKEN(Token::Kind::kComma) == Peek().combined()) {
      ConsumeToken(OfKind(Token::Kind::kComma));
      ASSERT_OK_OR_FAIL("error occured when consume comma in Parser::ParseTypeConstructor", nullptr);
      auto sequence_size = ParseNumericLiteral();
      ASSERT_OK_OR_FAIL("error occured when parse NumericLiteral in Parser::ParseTypeConstructor", nullptr);
      sequnce_size_recorder.emplace_front(stoi(sequence_size->copy_to_str()));
    } else {
      sequnce_size_recorder.emplace_front(-1);
    }
    ConsumeToken(OfKind(Token::Kind::kRightAngle));
    ASSERT_OK_OR_FAIL("error occured when consume RightAngle in Parser::ParseTypeConstructor", nullptr);
    ++sequence_count_right;
  }

  if (sequence_count_left != sequence_count_right) {
    ALOGE("sequence left and right is not equal");
    error_occured_flag_ = ErrorCode::ErrSequenceFormat;
    return nullptr;
  }

  return std::make_unique<raw::TypeConstructor>(scope.GetSourceElement(), std::move(components), sequnce_size_recorder);
}

std::vector<std::unique_ptr<raw::StructMember>> Parser::ParseStructMembers() {
  std::vector<std::unique_ptr<raw::StructMember>> members;

  while(true) {
    ASTScope scope(this);
    if (CASE_TOKEN(Token::Kind::kRightCurly) == Peek().combined()) {
      break;
    }

    // TODO: failsafe is not complete
    auto type = ParseTypeConstructor();
    ASSERT_OK_OR_FAIL("error occured when parse type in Parser::ParseStructMembers", members);

    auto name = ParseIdentifier();
    ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseStructMembers", members);

    ConsumeToken(OfKind(Token::Kind::kSemicolon));
    ASSERT_OK_OR_FAIL("error occured when consume semicolon in Parser::ParseStructMembers", members);

    members.emplace_back(std::make_unique<raw::StructMember>(scope.GetSourceElement(), std::move(type), std::move(name)));
  }
  return members;
}

std::vector<std::unique_ptr<raw::UnionMember>> Parser::ParseUnionMembers() {
  std::vector<std::unique_ptr<raw::UnionMember>> members;

  while(true) {
    ASTScope scope(this);
    bool is_default_case = false;
    std::unique_ptr<raw::NumericLiteral> case_value;

    switch (Peek().combined()) {
      // TODO: failsafe is not complete
      default:
        ALOGE("is not expected token in Parser::ParseUnionMembers");
        return members;
      case CASE_TOKEN(Token::Kind::kRightCurly):
        return members;
      case CASE_IDENTIFIER(Token::Subkind::kCase): {
        ConsumeToken(IdentifierOfSubkind(Token::Subkind::kCase));
        ASSERT_OK_OR_FAIL("error occured when consume case in Parser::ParseUnionMembers", members);
        case_value = std::move(ParseNumericLiteral());
        ASSERT_OK_OR_FAIL("error occured when parse case_value in Parser::ParseUnionMembers", members);
        break;
      }
      case CASE_IDENTIFIER(Token::Subkind::kDefault): {
        ConsumeToken(IdentifierOfSubkind(Token::Subkind::kDefault));
        ASSERT_OK_OR_FAIL("error occured when consume Default in Parser::ParseUnionMembers", members);
        is_default_case = true;
        break;
      }
    }

    // TODO: failsafe is not complete
    ConsumeToken(OfKind(Token::Kind::kColon));
    ASSERT_OK_OR_FAIL("error occured when consume colon in Parser::ParseUnionMembers", members);

    auto type = ParseTypeConstructor();
    ASSERT_OK_OR_FAIL("error occured when parse type in Parser::ParseUnionMembers", members);

    auto name = ParseIdentifier();
    ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseUnionMembers", members);

    ConsumeToken(OfKind(Token::Kind::kSemicolon));
    ASSERT_OK_OR_FAIL("error occured when consume semicolon in Parser::ParseUnionMembers", members);
  
    if (is_default_case) {
      members.emplace_back(std::make_unique<raw::UnionMember>(scope.GetSourceElement(), std::move(type), std::move(name)));
    } else {
      members.emplace_back(std::make_unique<raw::UnionMember>(scope.GetSourceElement(), std::move(type),
        std::move(name), std::move(case_value)));
    }
  }
  return members;
}

bool Parser::ParsePointValueInEnumMenber(int& value) {
  ConsumeToken(OfKind(Token::Kind::kAt));
  ASSERT_OK_OR_FAIL("error occured when consume at in Parser::ParsePointValueInEnumMenber", false);

  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kValue));
  ASSERT_OK_OR_FAIL("error occured when consume value in Parser::ParsePointValueInEnumMenber", false);

  ConsumeToken(OfKind(Token::Kind::kLeftParen));
  ASSERT_OK_OR_FAIL("error occured when consume leftParen in Parser::ParsePointValueInEnumMenber", false);

  auto point_value = ParseNumericLiteral();
  ASSERT_OK_OR_FAIL("error occured when parse pointValue in Parser::ParsePointValueInEnumMenber", false);

  ConsumeToken(OfKind(Token::Kind::kRightParen));
  ASSERT_OK_OR_FAIL("error occured when consume RightParen in Parser::ParsePointValueInEnumMenber", false);

  value = stoi(point_value->copy_to_str());
  return true;
}

std::vector<std::unique_ptr<raw::EnumMember>> Parser::ParseEnumMembers() {
  std::vector<std::unique_ptr<raw::EnumMember>> members;
  bool has_pointed_value = false;
  int value = 0;

  while (true) {
    ASTScope scope(this);

    switch (Peek().combined()) {
      // TODO: failsafe is not complete
      default:
        ALOGE("is not expected token in Parser::ParseEnumMembers");
        return members;
      case CASE_TOKEN(Token::Kind::kRightCurly):
        return members;
      case CASE_TOKEN(Token::Kind::kAt):
        if (ParsePointValueInEnumMenber(value)) {
          has_pointed_value = true;
        }
        break;
      case CASE_TOKEN(Token::Kind::kIdentifier):
        if (!has_pointed_value) {
          ++value;
        }
        auto name = ParseIdentifier();
        ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseEnumMembers", members);
        members.emplace_back(std::make_unique<raw::EnumMember>(scope.GetSourceElement(), std::move(name), value));
        has_pointed_value = false;
        if (CASE_TOKEN(Token::Kind::kComma) == Peek().combined()) {
          ConsumeToken(OfKind(Token::Kind::kComma));
          ASSERT_OK_OR_FAIL("error occured when consume comma in Parser::ParseEnumMembers", members);
        } else if (CASE_TOKEN(Token::Kind::kRightCurly) == Peek().combined()) {
          // do nothing, direct enter next loop.
        } else {
          return members;
        }
        break;
    }
  }
  return members;
}

std::vector<std::unique_ptr<raw::MethodParameter>> Parser::ParseInterfaceMethodParam() {
  std::vector<std::unique_ptr<raw::MethodParameter>> params;

  while (CASE_TOKEN(Token::Kind::kRightParen) != Peek().combined()) {
    ASTScope scope(this);

    if (CASE_IDENTIFIER(Token::Subkind::kIn) == Peek().combined() ||
        CASE_IDENTIFIER(Token::Subkind::kOut) == Peek().combined() ||
        CASE_IDENTIFIER(Token::Subkind::kInout) == Peek().combined()) {
          ConsumeToken(OfKind(Token::Kind::kIdentifier));
          ASSERT_OK_OR_FAIL("error occured when consume in/out/inout in Parser::ParseInterfaceMethodParam", params);
    }
    auto type = ParseTypeConstructor();
    ASSERT_OK_OR_FAIL("error occured when parse type in Parser::ParseInterfaceMethodParam", params);

    auto name = ParseIdentifier();
    ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseInterfaceMethodParam", params);

    if (CASE_TOKEN(Token::Kind::kRightParen) != Peek().combined()) {
      ConsumeToken(OfKind(Token::Kind::kComma));
      ASSERT_OK_OR_FAIL("error occured when consume comma in Parser::ParseInterfaceMethodParam", params);
    }
    params.emplace_back(std::make_unique<raw::MethodParameter>(scope.GetSourceElement(), std::move(name), std::move(type)));
  }
  return params;
}

std::unique_ptr<raw::MethodDeclaration> Parser::ParseInterfaceMethod() {
  ASTScope scope(this);

  std::vector<std::unique_ptr<raw::MethodReturn>> return_and_name_list;
  while (CASE_TOKEN(Token::Kind::kLeftParen) != Peek().combined()) {
    ASTScope scope(this);
    auto type = ParseTypeConstructor();
    return_and_name_list.emplace_back(std::make_unique<raw::MethodReturn>(scope.GetSourceElement(), std::move(type)));
    /***
     * TODO: 
     * Just surport one or zero return value at present.
     * If surport more return value, here maybe need parse a Separator.
     ***/
  }

  // return_and_name_list size must more than one because method must have "name"
  if (1 > return_and_name_list.size()) {
    ALOGE("Parser::ParseInterfaceMethod error occured: return_and_name_list less than 1");
    return nullptr;
  }

  // "name" must be consists of only one identifier
  if (1 != return_and_name_list.back()->type->components.size()) {
    ALOGE("Parser::ParseInterfaceMethod error occured: name identifier is not equal 1");
    return nullptr;
  }

  // last element in vector return_and_name_list is "name"
  auto name = std::move(std::move(return_and_name_list.back())->type->components.back());
  return_and_name_list.pop_back();

  ConsumeToken(OfKind(Token::Kind::kLeftParen));
  ASSERT_OK_OR_FAIL("error occured when consume leftParen in Parser::ParseInterfaceMethod", nullptr);

  auto parameters = ParseInterfaceMethodParam();
  ASSERT_OK_OR_FAIL("error occured when parse methodParam in Parser::ParseInterfaceMethod", nullptr);

  ConsumeToken(OfKind(Token::Kind::kRightParen));
  ASSERT_OK_OR_FAIL("error occured when consume rightParen in Parser::ParseInterfaceMethod", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when consume semicolon in Parser::ParseInterfaceMethod", nullptr);

  return std::make_unique<raw::MethodDeclaration>(scope.GetSourceElement(), std::move(name), std::move(return_and_name_list),
    std::move(parameters));
}

std::vector<std::unique_ptr<raw::EventMember>> Parser::ParseInterfaceEventMember() {
  std::vector<std::unique_ptr<raw::EventMember>> members;

  while (CASE_TOKEN(Token::Kind::kRightCurly) != Peek().combined()) {
    ASTScope scope(this);
    auto attribute = ParseIdentifier();
    ASSERT_OK_OR_FAIL("error occured when parse attribute in Parser::ParseInterfaceEventMember", members);

    auto type = ParseTypeConstructor();
    ASSERT_OK_OR_FAIL("error occured when parse type in Parser::ParseInterfaceEventMember", members);

    auto name = ParseIdentifier();
    ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseInterfaceEventMember", members);

    ConsumeToken(OfKind(Token::Kind::kSemicolon));
    ASSERT_OK_OR_FAIL("error occured when consume semicolon in Parser::ParseInterfaceEventMember", members);

    members.emplace_back(std::make_unique<raw::EventMember>(scope.GetSourceElement(), std::move(name),
      std::move(attribute), std::move(type)));
  }
  return members;
}

std::unique_ptr<raw::EventDeclaration> Parser::ParseInterfaceEvent() {
  ASTScope scope(this);

  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kEventtype));
  ASSERT_OK_OR_FAIL("error occured when consume eventtype in Parser::ParseInterfaceEvent", nullptr);

  auto name = ParseIdentifier();
  ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseInterfaceEvent", nullptr);

  ConsumeToken(OfKind(Token::Kind::kLeftCurly));
  ASSERT_OK_OR_FAIL("error occured when consume leftCurly in Parser::ParseInterfaceEvent", nullptr);

  auto members = ParseInterfaceEventMember();
  ASSERT_OK_OR_FAIL("error occured when parse evnetMember in Parser::ParseInterfaceEvent", nullptr);

  ConsumeToken(OfKind(Token::Kind::kRightCurly));
  ASSERT_OK_OR_FAIL("error occured when consume rightCurly in Parser::ParseInterfaceEvent", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when consume semicolon in Parser::ParseInterfaceEvent", nullptr);

  return std::make_unique<raw::EventDeclaration>(scope.GetSourceElement(), std::move(name), std::move(members));
}

/************ level 0 parse ****************/
std::unique_ptr<raw::CompoundIdentifier> Parser::ParseModuleNameCompound(ASTScope& scope) {
  std::vector<std::unique_ptr<raw::Identifier>> components;

  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kModule));
  ASSERT_OK_OR_FAIL("error occured when consume module in Parser::ParseModuleNameCompound", nullptr);

  while(true) {
    components.emplace_back(ParseIdentifier());
    if (CASE_TOKEN(Token::Kind::kDot) == Peek().combined()) {
      ConsumeToken(OfKind(Token::Kind::kDot));
      ASSERT_OK_OR_FAIL("error occured when consume dot in Parser::ParseModuleNameCompound", nullptr);
      continue;
    }
    break;
  }

  ConsumeToken(OfKind(Token::Kind::kLeftCurly));
  ASSERT_OK_OR_FAIL("error occured when consume LeftCurly in Parser::ParseModuleNameCompound", nullptr);

  return std::make_unique<raw::CompoundIdentifier>(scope.GetSourceElement(), std::move(components));
}

std::unique_ptr<raw::ConstDeclaration> Parser::ParseConstDeclaration(ASTScope& scope) {
  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kConst));
  ASSERT_OK_OR_FAIL("error occured when consume const in Parser::ParseConstDeclaration", nullptr);

  auto type = ParseTypeConstructor();
  ASSERT_OK_OR_FAIL("error occured when parse type in Parser::ParseConstDeclaration", nullptr);
  
  auto name = ParseIdentifier();
  ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseConstDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kEqual));
  ASSERT_OK_OR_FAIL("error occured when consume equal in Parser::ParseConstDeclaration", nullptr);

  auto constant = ParseConstant();
  ASSERT_OK_OR_FAIL("error occured when parse constant in Parser::ParseConstDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when parse semicolon in Parser::ParseConstDeclaration", nullptr);

  return std::make_unique<raw::ConstDeclaration>(scope.GetSourceElement(), std::move(type), std::move(name),
                                                 std::move(constant));
}

std::unique_ptr<raw::StructDeclaration> Parser::ParseStructDeclaration(ASTScope& scope) {
  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kStruct));
  ASSERT_OK_OR_FAIL("error occured when consume struct in Parser::ParseStructDeclaration", nullptr);
  
  auto name = ParseIdentifier();
  ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseStructDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kLeftCurly));
  ASSERT_OK_OR_FAIL("error occured when consume leftCurly in Parser::ParseStructDeclaration", nullptr);

  auto members = ParseStructMembers();
  ASSERT_OK_OR_FAIL("error occured when parse member in Parser::ParseStructDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kRightCurly));
  ASSERT_OK_OR_FAIL("error occured when consume rightCurly in Parser::ParseStructDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when consume semicolon in Parser::ParseStructDeclaration", nullptr);

  return std::make_unique<raw::StructDeclaration>(scope.GetSourceElement(), std::move(name), std::move(members));
}

std::unique_ptr<raw::UnionDeclaration> Parser::ParseUnionDeclaration(ASTScope& scope) {
  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kUnion));
  ASSERT_OK_OR_FAIL("error occured when consume union in Parser::ParseUnionDeclaration", nullptr);

  auto name = ParseIdentifier();
  ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseUnionDeclaration", nullptr);

  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kSwitch));
  ASSERT_OK_OR_FAIL("error occured when consume switch in Parser::ParseUnionDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kLeftParen));
  ASSERT_OK_OR_FAIL("error occured when consume leftParen in Parser::ParseUnionDeclaration", nullptr);

  auto select_type = ParseTypeConstructor();
  ASSERT_OK_OR_FAIL("error occured when parse select_type in Parser::ParseUnionDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kRightParen));
  ASSERT_OK_OR_FAIL("error occured when consume rightParen in Parser::ParseUnionDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kLeftCurly));
  ASSERT_OK_OR_FAIL("error occured when consume LeftCurly in Parser::ParseUnionDeclaration", nullptr);

  auto members = ParseUnionMembers();
  ASSERT_OK_OR_FAIL("error occured when parse member in Parser::ParseUnionDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kRightCurly));
  ASSERT_OK_OR_FAIL("error occured when consume RightCurly in Parser::ParseUnionDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when parse semicolon in Parser::ParseUnionDeclaration", nullptr);

  return std::make_unique<raw::UnionDeclaration>(scope.GetSourceElement(), std::move(name), std::move(members),
    std::move(select_type));
}

std::unique_ptr<raw::EnumDeclaration> Parser::ParseEnumDeclaration(ASTScope& scope) {
  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kEnum));
  ASSERT_OK_OR_FAIL("error occured when consume enum in Parser::ParseEnumDeclaration", nullptr);

  auto name = ParseIdentifier();
  ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseEnumDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kLeftCurly));
  ASSERT_OK_OR_FAIL("error occured when consume LeftCurly in Parser::ParseEnumDeclaration", nullptr);

  auto members = ParseEnumMembers();
  ASSERT_OK_OR_FAIL("error occured when parse member in Parser::ParseEnumDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kRightCurly));
  ASSERT_OK_OR_FAIL("error occured when consume RightCurly in Parser::ParseEnumDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when parse semicolon in Parser::ParseEnumDeclaration", nullptr);

  return std::make_unique<raw::EnumDeclaration>(scope.GetSourceElement(), std::move(name), std::move(members));
}

std::unique_ptr<raw::InterfaceDeclaration> Parser::ParseInterfaceDeclaration(ASTScope& scope,
  std::unique_ptr<raw::Identifier> attribute) {

  if (nullptr == attribute) {
    ALOGE("atrribute is nullptr in Parser::ParseInterfaceDeclaration");
    return nullptr;
  }

  std::vector<std::unique_ptr<raw::MethodDeclaration>> methods;
  std::vector<std::unique_ptr<raw::EventDeclaration>> events;

  ConsumeToken(IdentifierOfSubkind(Token::Subkind::kInterface));
  ASSERT_OK_OR_FAIL("error occured when consume interface in Parser::ParseInterfaceDeclaration", nullptr);

  auto name = ParseIdentifier();
  ASSERT_OK_OR_FAIL("error occured when parse name in Parser::ParseInterfaceDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kLeftCurly));
  ASSERT_OK_OR_FAIL("error occured when consume LeftCurly in Parser::ParseInterfaceDeclaration", nullptr);

  while (CASE_TOKEN(Token::Kind::kRightCurly) != Peek().combined()) {
    if (CASE_IDENTIFIER(Token::Subkind::kEventtype) == Peek().combined()) {
      auto event = ParseInterfaceEvent();
      if (nullptr != event) {
        events.emplace_back(std::move(event));
      }
    } else {
      auto method = ParseInterfaceMethod();
      if (nullptr != method) {
        methods.emplace_back(std::move(method));
      }
    }
  }

  ConsumeToken(OfKind(Token::Kind::kRightCurly));
  ASSERT_OK_OR_FAIL("error occured when consume RightCurly in Parser::ParseInterfaceDeclaration", nullptr);

  ConsumeToken(OfKind(Token::Kind::kSemicolon));
  ASSERT_OK_OR_FAIL("error occured when parse semicolon in Parser::ParseInterfaceDeclaration", nullptr);

  return std::make_unique<raw::InterfaceDeclaration>(scope.GetSourceElement(), std::move(name),
            std::move(attribute), std::move(methods), std::move(events));
}

std::unique_ptr<raw::File> Parser::ParseFile() {
  ASTScope scope(this);

  std::vector<std::unique_ptr<raw::ConstDeclaration>> const_declaration_list;
  std::vector<std::unique_ptr<raw::StructDeclaration>> struct_declaration_list;
  std::vector<std::unique_ptr<raw::UnionDeclaration>> union_declaration_list;
  std::vector<std::unique_ptr<raw::EnumDeclaration>> enum_declaration_list;
  std::vector<std::unique_ptr<raw::InterfaceDeclaration>> interface_declaration_list;

  std::unique_ptr<raw::CompoundIdentifier> module_name_compound;

  std::unique_ptr<raw::Identifier> last_interface_attribute;

  auto parse_declaration = [&]() {
    ASTScope scope(this);

    switch (Peek().combined()) {
      default:
        ALOGD("ErrExpectedDeclaratio: %s", std::string(last_token_.data()).c_str());
        ConsumeToken([](Token::KindAndSubkind actual) -> ErrorCode { return ErrorCode::NoError;});
        return More;

      case CASE_TOKEN(Token::Kind::kEndOfFile):
        return Done;

      case CASE_TOKEN(Token::Kind::kAt):
        ConsumeToken(OfKind(Token::Kind::kAt));
        last_interface_attribute = ParseIdentifier();
        ASSERT_OK_OR_FAIL("error occured when parse last_interface_attribute", More);
        return More;

      case CASE_TOKEN(Token::Kind::kRightCurly):
        ConsumeToken(OfKind(Token::Kind::kRightCurly));
        return More;

      case CASE_IDENTIFIER(Token::Subkind::kModule): {
        // TODO: should consider meeting "module" multiple times
        module_name_compound = ParseModuleNameCompound(scope);
        return More;
      }

      case CASE_IDENTIFIER(Token::Subkind::kConst): {
        add(&const_declaration_list,
            [&] { return ParseConstDeclaration(scope); });
        return More;
      }

      case CASE_IDENTIFIER(Token::Subkind::kStruct): {
        add(&struct_declaration_list,
            [&] { return ParseStructDeclaration(scope); });
        return More;
      }

      case CASE_IDENTIFIER(Token::Subkind::kUnion): {
        add(&union_declaration_list,
            [&] { return ParseUnionDeclaration(scope); });
        return More;
      }

      case CASE_IDENTIFIER(Token::Subkind::kEnum): {
        add(&enum_declaration_list,
            [&] { return ParseEnumDeclaration(scope); });
        return More;
      }

      case CASE_IDENTIFIER(Token::Subkind::kInterface): {
        add(&interface_declaration_list,
            [&] { return ParseInterfaceDeclaration(scope, std::move(last_interface_attribute)); });
        return More;
      }
    }
  };

  while (parse_declaration() == More) {
    //
  }
#ifdef DEBUG_MODE
  // const
  for(auto& const_declaration : const_declaration_list) {
    std::string type_string("");
    for (auto& type : const_declaration->type->components) {
      type_string = type_string + " " + type->copy_to_str();
    }
    ALOGD("const_declaration:%s", const_declaration->copy_to_str().c_str());
    ALOGD("const name:%s", const_declaration->name->copy_to_str().c_str());
    ALOGD("const type:%s", type_string.c_str());
    ALOGD("const constant:%s", const_declaration->constant->copy_to_str().c_str());
    ALOGD(" ");
  }
  // struct
  for(auto& struct_declaration : struct_declaration_list) {
    ALOGD("struct_declaration:%s", struct_declaration->copy_to_str().c_str());
    ALOGD("struct name:%s", struct_declaration->name->copy_to_str().c_str());
    int i = 0;
    for (auto& member : struct_declaration->members) {
      i++;
      std::string type_string("");
      for (auto& type : member->type->components) {
        type_string = type_string + " " + type->copy_to_str();
      }
      std::string seq_string("");
      for (int i = 0; i < member->type->sequence_size_recorder.size(); i++) {
        seq_string = seq_string + " " + std::to_string(member->type->sequence_size_recorder[i]);
      }
        ALOGD("struct member, member index:%d, member name:%s, member type:%s, seq size:%s",
         i, member->name->copy_to_str().c_str(), type_string.c_str(), seq_string.c_str());
    }
    ALOGD(" ");
  }
#endif
  std::optional<Token> end = ConsumeToken(OfKind(Token::Kind::kEndOfFile));
  ASSERT_OK_OR_FAIL("error occured when consume EOF in Parser::ParseFile", nullptr);

  return std::make_unique<raw::File>(
      scope.GetSourceElement(), end.value(), std::move(module_name_compound), std::move(const_declaration_list),
      std::move(struct_declaration_list), std::move(union_declaration_list), std::move(enum_declaration_list),
      std::move(interface_declaration_list), std::move(tokens_));
}

}  // namespace idlc
