
#ifndef _ONE_IDLC_COMPILED_AST_H_
#define _ONE_IDLC_COMPILED_AST_H_

#include <unordered_map>
#include <set>

#include "raw_ast.h"
#include "log.h"


namespace idlc {

class CompiledAST {
 public:
  CompiledAST(std::unique_ptr<raw::File> raw_ast)
    : raw_ast_(std::move(raw_ast)) {}

  bool Compile();


  std::unique_ptr<raw::File> raw_ast_;
  std::vector<const raw::SourceElement*> declaration_order_;

 private:
  bool RegisterAllDeclarations();
  bool TopolSortDeclarations();
  bool DeclDependencies(const raw::SourceElement* decl, std::set<const raw::SourceElement*>* out_edges);

  std::unordered_map<std::string, raw::SourceElement*> declarations_;
};
}  // namespace idlc

#endif  // _ONE_IDLC_COMPILED_AST_H_