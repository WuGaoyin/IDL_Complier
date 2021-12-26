
#include "json_generator.h"

namespace idlc {

void JSONGenerator::GenerateTypeName(int index, const raw::TypeConstructor& type) {
  // Recursive exit
  if (index >= static_cast<int>(type.sequence_size_recorder.size()) - 1) {
    std::vector<std::string> type_name_list;
    for (const auto& component : type.components) {
        type_name_list.push_back(component->copy_to_str());
    }
    GenerateObjectMember("type_name", type_name_list, Position::kFirst);
    if (!type.sequence_size_recorder.empty()) {
      GenerateObjectMember("sequence_size", static_cast<int64_t>(type.sequence_size_recorder[index]));
    }
    Outdent();
    EmitNewlineWithIndent();
    return;
  }

  GenerateObjectPunctuation(Position::kFirst);
  EmitObjectKey("type_name");
  EmitObjectBegin();

  // Recursive start
  GenerateTypeName(++index, type);

  EmitObjectEnd();
  GenerateObjectMember("sequence_size", static_cast<int64_t>(type.sequence_size_recorder[--index]));
  Outdent();
  EmitNewlineWithIndent();
}

void JSONGenerator::Generate(const raw::TypeConstructor& value) {
  GenerateObject([&]() {
    GenerateTypeName(0, value);
  });
}

void JSONGenerator::Generate(const raw::Constant* value) {
  // only LiteralConstant at present
  auto literal_constant = static_cast<const raw::LiteralConstant*>(value);
  switch (literal_constant->literal->kind) {
  case raw::Literal::Kind::kFalse:
    Generate(false);
    break;
  case raw::Literal::Kind::kTrue:
    Generate(true);
    break;
  case raw::Literal::Kind::kString: {
    auto string_literal = static_cast<raw::StringLiteral*>(literal_constant->literal.get());
    Generate(string_literal->MakeContents());
    break;
  }
  case raw::Literal::Kind::kNumeric:
    Generate(static_cast<int64_t>(stoi(literal_constant->literal->copy_to_str())));
    break;

  default:
    break;
  }
}

void JSONGenerator::Generate(const raw::ConstDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("type", value.type);
    GenerateObjectMember("value", value.constant.get());
  });
}

void JSONGenerator::Generate(const raw::EnumDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("members", value.members);
  });
}

void JSONGenerator::Generate(const raw::EnumMember& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("value", static_cast<int64_t>(value.value));
  });
}

void JSONGenerator::Generate(const raw::StructDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("members", value.members);
  });
}

void JSONGenerator::Generate(const raw::StructMember& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("type", value.type);
  });
}

void JSONGenerator::Generate(const raw::UnionDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    std::vector<std::string> select_type_list;
    for (const auto& component : value.select_type->components) {
        select_type_list.push_back(component->copy_to_str());
    }
    GenerateObjectMember("select_type", select_type_list);
    GenerateObjectMember("members", value.members);
  });
}

void JSONGenerator::Generate(const raw::UnionMember& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    if (!value.is_default_member) {
      GenerateObjectMember("case_value", static_cast<int64_t>(stoi(value.case_value->copy_to_str())));
    }
    GenerateObjectMember("type", value.type);
  });
}

void JSONGenerator::Generate(const raw::InterfaceDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("attribute", value.attribute->copy_to_str());
    GenerateObjectMember("method_list", value.methods);
    GenerateObjectMember("event_list", value.events);
  });
}

void JSONGenerator::Generate(const raw::MethodDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("method_name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("method_return", value.returns);
    GenerateObjectMember("method_parameter", value.parameters);
  });
}

void JSONGenerator::Generate(const raw::MethodReturn& value) {
  GenerateObject([&]() {
    GenerateObjectMember("type", value.type, Position::kFirst);
  });
}

void JSONGenerator::Generate(const raw::MethodParameter& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("type", value.type);
  });
}

void JSONGenerator::Generate(const raw::EventDeclaration& value) {
  GenerateObject([&]() {
    GenerateObjectMember("event_name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("members", value.members);
  });
}

void JSONGenerator::Generate(const raw::EventMember& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name->copy_to_str(), Position::kFirst);
    GenerateObjectMember("type", value.type);
    GenerateObjectMember("attribute", value.attribute->copy_to_str());
  });
}

void JSONGenerator::Generate(const JSONGenerator::DeclaNameAndType& value) {
  GenerateObject([&]() {
    GenerateObjectMember("name", value.name, Position::kFirst);
    GenerateObjectMember("category", value.type);
  });
}

std::vector<JSONGenerator::DeclaNameAndType> JSONGenerator::GetDeclaNameAndType() {
  std::vector<JSONGenerator::DeclaNameAndType> decla_name_and_type_list;
  for (auto& decl : compiled_ast_->declaration_order_) {
    switch (decl->kind_) {
      case raw::SourceElement::Kind::kEnum: {
        auto enum_decl = static_cast<const raw::EnumDeclaration*>(decl);
        JSONGenerator::DeclaNameAndType decla_name_and_type{enum_decl->name->copy_to_str(), std::string("enum")};
        decla_name_and_type_list.emplace_back(decla_name_and_type);
        break;
      }

      case raw::SourceElement::Kind::kConst: {
        auto const_decl = static_cast<const raw::ConstDeclaration*>(decl);
        JSONGenerator::DeclaNameAndType decla_name_and_type{const_decl->name->copy_to_str(), std::string("const")};
        decla_name_and_type_list.emplace_back(decla_name_and_type);
        break;
      }

      case raw::SourceElement::Kind::kStruct: {
        auto struct_decl = static_cast<const raw::StructDeclaration*>(decl);
        JSONGenerator::DeclaNameAndType decla_name_and_type{struct_decl->name->copy_to_str(), std::string("struct")};
        decla_name_and_type_list.emplace_back(decla_name_and_type);
        break;
      }

      case raw::SourceElement::Kind::kUnion: {
        auto union_decl = static_cast<const raw::UnionDeclaration*>(decl);
        JSONGenerator::DeclaNameAndType decla_name_and_type{union_decl->name->copy_to_str(), std::string("union")};
        decla_name_and_type_list.emplace_back(decla_name_and_type);
        break;
      }

      case raw::SourceElement::Kind::kInterface: {
        auto interface_decl = static_cast<const raw::InterfaceDeclaration*>(decl);
        JSONGenerator::DeclaNameAndType decla_name_and_type{interface_decl->name->copy_to_str(), std::string("interface")};
        decla_name_and_type_list.emplace_back(decla_name_and_type);
        break;
      }
    }
  }
  return decla_name_and_type_list;
}

std::ostringstream JSONGenerator::Produce() {
  ResetIndentLevel();
  GenerateObject([&]() {
    GenerateObjectMember("version", std::string_view("0.0.1"), Position::kFirst);

    std::vector<std::string> module_name_list;
    for (const auto& module : compiled_ast_->raw_ast_->module_name->components) {
        module_name_list.push_back(module->copy_to_str());
    }
    GenerateObjectMember("module_name", module_name_list);
    GenerateObjectMember("const_declarations", compiled_ast_->raw_ast_->const_declaration_list);
    GenerateObjectMember("enum_declarations", compiled_ast_->raw_ast_->enum_declaration_list);
    GenerateObjectMember("struct_declarations", compiled_ast_->raw_ast_->struct_declaration_list);
    GenerateObjectMember("union_declarations", compiled_ast_->raw_ast_->union_declaration_list);
    GenerateObjectMember("interface_declarations", compiled_ast_->raw_ast_->interface_declaration_list);
    GenerateObjectMember("declarations_order", GetDeclaNameAndType());
  });

  GenerateEOF();
  return std::move(json_file_);
}

}  // namespace idlc