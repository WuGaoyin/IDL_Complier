import os
import logging
import sys
from string import Template
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils

class CppGeneratorProtocol(object):
    type_mapping = {
        "void":"void",
        "boolean":"bool",
        "int8":"int8_t",
        "uint8":"uint8_t",
        "short":"int16_t",
        "long":"int32_t",
        "long long":"int64_t",
        "unsigned short":"uint16_t",
        "unsigned long":"uint32_t",
        "unsigned long long":"uint64_t",
        "float":"float",
        "double":"double",
        "string":"std::string",
    }

    def __init__(self, path, base_name, file, ir_dict):
        self._ir_dict = ir_dict
        self._path = path
        self._base_name = base_name
        self._file = file
        self._moudle_list = []
        self._head_include_prefix = ""
        self._full_name_space = ""

        if ir_parser.kModule in ir_dict.keys():
            self._moudle_list = ir_dict[ir_parser.kModule]
            for item in self._moudle_list:
                if not self._head_include_prefix == "":
                    self._head_include_prefix += "_"
                self._head_include_prefix += item.upper()

                if not self._full_name_space == "": 
                    self._full_name_space += "."
                self._full_name_space += item

        utils.Utils.createGenFile(self._path, self._file)

    def _genNameSpaceStart(self):
        content_lines = ""
        for item in self._moudle_list:
            content_lines += """
namespace %s {""" % (item)

        content_lines += "\n"
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def _genNameSpaceEnd(self):
        content_lines = ""
        for item in self._moudle_list:
            content_lines += """
}  // namespace %s""" % (item)

        content_lines += "\n"
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def _genHeadFileStart(self, tail):
        content = """#ifndef %s_%s_%s_H_
#define %s_%s_%s_H_
        """ % (self._head_include_prefix, self._base_name.upper(), tail, self._head_include_prefix, self._base_name.upper(), tail)
        utils.Utils.writeGenFile(self._path, self._file, content)

    def _genHeadFileEnd(self, tail):
        content = """\n\n#endif  // %s_%s_%s_H_""" % (self._head_include_prefix, self._base_name.upper(), tail)
        utils.Utils.writeGenFile(self._path, self._file, content)

    def _isArgsListEmpty(self, args_list):
        if len(args_list) == 0:
            return True

        for arg in args_list:
            if self._isArgEmpty(arg):
                return True
        return False

    def _isArgEmpty(self, arg):
        if not ir_parser.kType in arg.keys():
            utils.ColorsPrint.PrintE("there is no key of type")
            return True
        type = self._typeConvert(arg[ir_parser.kType])

        if type == "void":
            return True

        return False

    # if type is empty, return void
    def _typeConvert(self, type):
        #has not subtype with 'sequence'
        if isinstance (type, list):
            idl_type = ""
            for item in type:
                if idl_type == "":
                    idl_type = item
                else:
                    idl_type += " " + item
                
            if item in CppGeneratorProtocol.type_mapping.keys():
                basic_type = CppGeneratorProtocol.type_mapping[idl_type]
            else:
                basic_type = idl_type

            if basic_type == "":
                return "void"

            return basic_type 
        #has subtype with 'sequence'            
        else:
            if not ir_parser.kTypeName in type.keys():
                print("invalid format")
                print(type)
                sys.exit(1)

            sequence_size = 0
            if ir_parser.kSequenceSize in type.keys():
                # it's a vector or array  
                sequence_size = type[ir_parser.kSequenceSize]

            if sequence_size > 0:
                type_str = """std::array<%s, %d>""" % (self._typeConvert(type[ir_parser.kTypeName]), sequence_size)
            elif sequence_size == -1:
                type_str = """std::vector<%s>""" % (self._typeConvert(type[ir_parser.kTypeName]))
            else:
                type_str = self._typeConvert(type[ir_parser.kTypeName])

        return  type_str

    def _getArgListStr(self, direction, arg_list, begin, middle, end):
        args_str = ""

        i = 0
        for arg in arg_list:
            arg_name = ""
            if ir_parser.kName in arg.keys():
                arg_name = arg[ir_parser.kName]
            else:
                 # if arg name is not provided, use arg name as in_arg_N, or out_arg_N
                # for exammple, out_arg_0
                arg_name = "%s_arg_%d" %(direction, i)

            arg_type = self._typeConvert(arg[ir_parser.kType])
            if arg_type == "void":
                return ""
            if args_str == "":
                args_str += begin + arg_type + middle + " " + arg_name
            else:
                args_str += end + begin + arg_type + middle + " " + arg_name   
            i += 1
        return args_str

    def _getNoTypeArgListStr(self, direction, arg_list, begin, end):
        args_str = ""

        i = 0
        for arg in arg_list:
            arg_name = ""
            if ir_parser.kName in arg.keys():
                arg_name = arg[ir_parser.kName]
            else:
                # if arg name is not provided, use arg name as IN_arg_N, or in_arg_N
                # for exammple, out_arg_0
                arg_name = "%s_arg_%d" %(direction, i)

            arg_type = self._typeConvert(arg[ir_parser.kType])
            if arg_type == "void":
                return ""
            
            if args_str == "":
                args_str += begin + arg_name
            else:
                args_str += end + begin + arg_name 
            i += 1
        return args_str

    def _getMethodEventNamesStr(self, interface_info, begin, end):
        result = ""
        if ir_parser.kMethodList in interface_info.keys():
            for method in interface_info[ir_parser.kMethodList]:
                if result == "":
                    result += begin + "\"" + method[ir_parser.kMethodName] + "\""
                else:
                    result += end + begin + "\"" + method[ir_parser.kMethodName] + "\""
                
        if ir_parser.kEventList in interface_info.keys():
            for event in interface_info[ir_parser.kEventList]:
                if result == "":
                    result += begin + "\"" + event[ir_parser.kEventName] + "\""
                else:
                    result += end + begin + "\"" + event[ir_parser.kEventName] + "\""
        return result







   



