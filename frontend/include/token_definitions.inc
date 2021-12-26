
// No header inclusion guards _sic_ as this may be re-included with
// different X macro arguments.

#if !defined(TOKEN)
#define TOKEN(Name)
#endif

#if !defined(TOKEN_SUBKIND)
#define TOKEN_SUBKIND(Name, Spelling)
#endif

// Control and whitespace
TOKEN(NotAToken)
TOKEN(EndOfFile)
TOKEN(Comment)
TOKEN(DocComment)

// Identifiers and constants
TOKEN(Identifier)
TOKEN(NumericLiteral)
TOKEN(StringLiteral)

// Punctuation.
TOKEN(LeftParen)
TOKEN(RightParen)
TOKEN(LeftSquare)
TOKEN(RightSquare)
TOKEN(LeftCurly)
TOKEN(RightCurly)
TOKEN(LeftAngle)
TOKEN(RightAngle)

TOKEN(At)
TOKEN(Dot)
TOKEN(Comma)
TOKEN(Semicolon)
TOKEN(Colon)
TOKEN(Question)
TOKEN(Equal)
TOKEN(Ampersand)
TOKEN(Arrow)
TOKEN(Pipe)

// Token subkinds
TOKEN_SUBKIND(Array, "array")
TOKEN_SUBKIND(Request, "request")
TOKEN_SUBKIND(String, "string")
TOKEN_SUBKIND(Vector, "vector")

// TOKEN_SUBKIND(Max, "MAX")
// MAX is not a real keyword, but it effectively works like one. It can be used
// as vector<T>:MAX or string:MAX. See Library::ResolveSizeBound in flat_ast.cc.

TOKEN_SUBKIND(Bits, "bits")
TOKEN_SUBKIND(Const, "const")
TOKEN_SUBKIND(Enum, "enum")
TOKEN_SUBKIND(Service, "service")
TOKEN_SUBKIND(Struct, "struct")
TOKEN_SUBKIND(Type, "type")
TOKEN_SUBKIND(Union, "union")

TOKEN_SUBKIND(Error, "error")

TOKEN_SUBKIND(True, "true")
TOKEN_SUBKIND(False, "false")

TOKEN_SUBKIND(Sequence, "sequence")
TOKEN_SUBKIND(Interface, "interface")
TOKEN_SUBKIND(Eventtype, "eventtype")

TOKEN_SUBKIND(Module, "module")
TOKEN_SUBKIND(Unsigned, "unsigned")
TOKEN_SUBKIND(Long, "long")
TOKEN_SUBKIND(Short, "short")
TOKEN_SUBKIND(Switch, "switch")
TOKEN_SUBKIND(Case, "case")
TOKEN_SUBKIND(Default, "default")
TOKEN_SUBKIND(Value, "value")

TOKEN_SUBKIND(In, "in")
TOKEN_SUBKIND(Out, "out")
TOKEN_SUBKIND(Inout, "inout")

#undef TOKEN_SUBKIND
#undef TOKEN
