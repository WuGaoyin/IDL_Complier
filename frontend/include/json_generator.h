
#ifndef _ONE_IDLC_JSON_GENERATOR_H_
#define _ONE_IDLC_JSON_GENERATOR_H_

#include <sstream>
#include <string>
#include <vector>

#include "json_writer.h"
#include "compiled_ast.h"
namespace idlc {

class JSONGenerator:public utils::JsonWriter<JSONGenerator>{
 public:
  using utils::JsonWriter<JSONGenerator>::Generate;
  using utils::JsonWriter<JSONGenerator>::GenerateArray;

  JSONGenerator(CompiledAST* compiled_ast)
    : JsonWriter(json_file_), compiled_ast_(compiled_ast) {}

  std::ostringstream Produce();

  struct DeclaNameAndType {
    std::string name;
    std::string type;
  };

  void GenerateTypeName(int index, const raw::TypeConstructor& type);

  void Generate(const raw::TypeConstructor& value);
  void Generate(const raw::ConstDeclaration& value);
  void Generate(const raw::EnumDeclaration& value);
  void Generate(const raw::EnumMember& value);
  void Generate(const raw::StructDeclaration& value);
  void Generate(const raw::StructMember& value);
  void Generate(const raw::UnionDeclaration& value);
  void Generate(const raw::UnionMember& value);
  void Generate(const raw::InterfaceDeclaration& value);
  void Generate(const raw::MethodDeclaration& value);
  void Generate(const raw::MethodReturn& value);
  void Generate(const raw::MethodParameter& value);
  void Generate(const raw::EventDeclaration& value);
  void Generate(const raw::EventMember& value);
  void Generate(const DeclaNameAndType& value);


  void Generate(const raw::Constant* value);
  std::vector<DeclaNameAndType> GetDeclaNameAndType();

 private:

  const CompiledAST* compiled_ast_;
  std::ostringstream json_file_;
};

}  // namespace idlc

#endif  // _ONE_IDLC_JSON_GENERATOR_H_