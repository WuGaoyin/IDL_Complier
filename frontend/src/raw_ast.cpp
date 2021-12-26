
// This file contains the implementations of the Accept methods for the AST
// nodes.  Generally, all they do is invoke the appropriate TreeVisitor method
// for each field of the node.

#include "raw_ast.h"

#include <map>

#include "tree_visitor.h"

namespace idlc {
namespace raw {

SourceElementMark::SourceElementMark(TreeVisitor* tv, const SourceElement& element)
    : tv_(tv), element_(element) {
  // tv_->OnSourceElementStart(element_);
}

SourceElementMark::~SourceElementMark() { /* tv_->OnSourceElementEnd(element_); */}

void File::Accept(TreeVisitor* visitor) const {
  SourceElementMark sem(visitor, *this);
#if 0
  visitor->OnLibraryDecl(library_decl);
  for (auto& i : using_list) {
    visitor->OnUsing(i);
  }
  for (auto& i : const_declaration_list) {
    visitor->OnConstDeclaration(i);
  }
  for (auto& i : protocol_declaration_list) {
    visitor->OnProtocolDeclaration(i);
  }
  for (auto& i : resource_declaration_list) {
    visitor->OnResourceDeclaration(i);
  }
  for (auto& i : service_declaration_list) {
    visitor->OnServiceDeclaration(i);
  }
  for (auto& i : type_decls) {
    visitor->OnTypeDecl(i);
  }
#endif
}

}  // namespace raw
}  // namespace idlc
