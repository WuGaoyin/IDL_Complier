
import logging
import json

kVersion = "version"
kModule = "module_name"
kEnumDeclarations = "enum_declarations"
kConstDeclarations = "const_declarations"
kStructDeclarations = "struct_declarations"
kUnionDeclarations = "union_declarations"
kInterfaceDeclarations = "interface_declarations"
kDeclarationsOrder = "declarations_order"
kEnum = "enum"
kConst = "const"
kStruct = "struct"
kUnion = "union"
kInterface = "interface"
kName = "name"
kType = "type"
kValue = "value"
kTypeName = "type_name"
kCategory = "category"
kMembers = "members"
kMemberType = "member_type"
kCaseValue= "case_value"
kSequenceSize= "sequence_size"
kMethodList = "method_list"
kEventList = "event_list"
kMethodName= "method_name"
kEventName= "event_name"
kMethodReturn= "method_return"
kMethodParameter= "method_parameter"

class JsonIRParser:
    def __init__(self, ir_path):
        self.__ir_path = ir_path

    def parse(self):
        ir_dict = {}

        with open(self.__ir_path) as f:
            ir_dict = json.load(f)

        return 0, ir_dict

