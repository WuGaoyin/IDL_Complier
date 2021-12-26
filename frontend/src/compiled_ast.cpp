
#include <algorithm>

#include "compiled_ast.h"

namespace idlc {

bool CompiledAST::RegisterAllDeclarations() {
  for (auto& const_declar : raw_ast_->const_declaration_list) {
    std::string name = const_declar->name->copy_to_str();
    auto it = declarations_.emplace(name, const_declar.get());
    if (!it.second) {
      ALOGE("declarations_ emplace a const_declar failed");
    }
  }

  for (auto& struct_declar : raw_ast_->struct_declaration_list) {
    std::string name = struct_declar->name->copy_to_str();
    auto it = declarations_.emplace(name, struct_declar.get());
    if (!it.second) {
      ALOGE("declarations_ emplace a struct_declar failed");
    }
  }

  for (auto& union_declar : raw_ast_->union_declaration_list) {
    std::string name = union_declar->name->copy_to_str();
    auto it = declarations_.emplace(name, union_declar.get());
    if (!it.second) {
      ALOGE("declarations_ emplace a union_declar failed");
    }
  }

  for (auto& enum_declar : raw_ast_->enum_declaration_list) {
    std::string name = enum_declar->name->copy_to_str();
    auto it = declarations_.emplace(name, enum_declar.get());
    if (!it.second) {
      ALOGE("declarations_ emplace a enum_declar failed");
    }
  }

  for (auto& interface_declar : raw_ast_->interface_declaration_list) {
    std::string name = interface_declar->name->copy_to_str();
    auto it = declarations_.emplace(name, interface_declar.get());
    if (!it.second) {
      ALOGE("declarations_ emplace a interface_declar failed");
    }
  }

  return true;
}

bool CompiledAST::DeclDependencies(const raw::SourceElement* decl, std::set<const raw::SourceElement*>* out_edges) {
  std::vector<std::string> build_in_type{"boolean", "int8", "uint8", "short", "long", "unsigned",
                                         "float", "double", "string", "sequence", "void"};

  std::set<const raw::SourceElement*> edges;

  auto parse_single_type_dependencies = [&](raw::Identifier* single_type) {
    if (std::find(build_in_type.begin(), build_in_type.end(), single_type->copy_to_str()) != build_in_type.end()) {
      return true;
    }
    if (0 == declarations_.count(single_type->copy_to_str())) {
      ALOGE("undefined declare used: %s", single_type->copy_to_str().c_str());
      return false;
    }
    edges.insert(declarations_.at(single_type->copy_to_str()));
    return true;
  };

  switch (decl->kind_) {
    case raw::SourceElement::Kind::kConst: {
      auto const_decl = static_cast<const raw::ConstDeclaration*>(decl);
      for (auto& single_type : const_decl->type->components) {
        if (!parse_single_type_dependencies(single_type.get())) {
          return false;
        }
      }
      break;
    }

    case raw::SourceElement::Kind::kStruct: {
      auto struct_decl = static_cast<const raw::StructDeclaration*>(decl);
      for (auto& member : struct_decl->members) {
        for (auto& single_type : member->type->components) {
          if (!parse_single_type_dependencies(single_type.get())) {
            return false;
          }
        }
      }
      break;
    }

    case raw::SourceElement::Kind::kUnion: {
      auto union_decl = static_cast<const raw::UnionDeclaration*>(decl);

      for (auto& single_type : union_decl->select_type->components) {
        if (!parse_single_type_dependencies(single_type.get())) {
          return false;
        }
      }
      for (auto& member : union_decl->members) {
        for (auto& single_type : member->type->components) {
          if (!parse_single_type_dependencies(single_type.get())) {
            return false;
          }
        }
      }
      break;
    }

    case raw::SourceElement::Kind::kInterface: {
      auto interface_decl = static_cast<const raw::InterfaceDeclaration*>(decl);

      for (auto& method : interface_decl->methods) {
        for (auto& method_return : method->returns) {
          for (auto& single_type : method_return->type->components) {
            if (!parse_single_type_dependencies(single_type.get())) {
              return false;
            }
          }
        }

        for (auto& parameter : method->parameters) {
          for (auto& single_type : parameter->type->components) {
            if (!parse_single_type_dependencies(single_type.get())) {
              return false;
            }
          }
        }
      }

      for (auto& event : interface_decl->events) {
        for (auto& member : event->members) {
          for (auto& single_type : member->type->components) {
            if (!parse_single_type_dependencies(single_type.get())) {
              return false;
            }
          }
        }
      }
      break;
    }

    case raw::SourceElement::Kind::kEnum:
    default:
        break;
  }

  *out_edges = std::move(edges);
  return true;
}

bool CompiledAST::TopolSortDeclarations() {
  // |degree| is the number of undeclared dependencies for each decl.
  std::unordered_map<const raw::SourceElement*, uint32_t> degrees;
  // |inverse_dependencies| records the decls that depend on each decl.
  std::unordered_map<const raw::SourceElement*, std::vector<const raw::SourceElement*>> inverse_dependencies;

  for (auto& name_and_decl : declarations_) {
    const raw::SourceElement* decl = name_and_decl.second;
    std::set<const raw::SourceElement*> deps;
    if (!DeclDependencies(decl, &deps)) {
      return false;
    }
    degrees[decl] = static_cast<uint32_t>(deps.size());
    for (const raw::SourceElement* dep : deps) {
      inverse_dependencies[dep].push_back(decl);
    }
  }

  // Start with all decls that have no incoming edges.
  std::vector<const raw::SourceElement*> decls_without_deps;
  for (const auto& decl_and_degree : degrees) {
    if (decl_and_degree.second == 0u) {
      decls_without_deps.push_back(decl_and_degree.first);
    }
  }

  while (!decls_without_deps.empty()) {
    // Pull one out of the queue.
    auto decl = decls_without_deps.back();
    decls_without_deps.pop_back();
    assert(degrees[decl] == 0u);
    declaration_order_.push_back(decl);

    // Decrement the incoming degree of all the other decls it
    // points to.
    auto& inverse_deps = inverse_dependencies[decl];
    for (const raw::SourceElement* inverse_dep : inverse_deps) {
      uint32_t& degree = degrees[inverse_dep];
      assert(degree != 0u);
      degree -= 1;
      if (degree == 0u)
        decls_without_deps.push_back(inverse_dep);
    }
  }

  if (declaration_order_.size() != degrees.size()) {
    ALOGE("We didn't visit all the edges when topologically sort! There was a cycle");
    return false;
  }

  return true;
  
}

bool CompiledAST::Compile() {
  RegisterAllDeclarations();

  TopolSortDeclarations();
#ifdef DEBUG_MODE
  ALOGE("topological sort:");
  for (auto& decl : declaration_order_) {
    switch (decl->kind_) {
      case raw::SourceElement::Kind::kEnum: {
        auto enum_decl = static_cast<const raw::EnumDeclaration*>(decl);
        ALOGE("name: %s", enum_decl->name->copy_to_str().c_str());
        break;
      }

      case raw::SourceElement::Kind::kConst: {
        auto const_decl = static_cast<const raw::ConstDeclaration*>(decl);
        ALOGE("name: %s", const_decl->name->copy_to_str().c_str());
        break;
      }

      case raw::SourceElement::Kind::kStruct: {
        auto struct_decl = static_cast<const raw::StructDeclaration*>(decl);
        ALOGE("name: %s", struct_decl->name->copy_to_str().c_str());
        break;
      }

      case raw::SourceElement::Kind::kUnion: {
        auto union_decl = static_cast<const raw::UnionDeclaration*>(decl);
        ALOGE("name: %s", union_decl->name->copy_to_str().c_str());
        break;
      }

      case raw::SourceElement::Kind::kInterface: {
        auto interface_decl = static_cast<const raw::InterfaceDeclaration*>(decl);
        ALOGE("name: %s", interface_decl->name->copy_to_str().c_str());
        break;
      }
    }
  }
#endif
  return true;
}

}  // namespace idlc