import logging
from string import Template
import sys
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
from . import cpp_gen_protocol


class CppCommonHeaderGenerator(cpp_gen_protocol.CppGeneratorProtocol):
    def __init__(self, path, base_name, ir_dict):
        super(CppCommonHeaderGenerator, self).__init__(path, base_name, base_name + "Common.h", ir_dict)

    def gen(self):
        print("start to gen common header")
        self._genHeadFileStart("COMMON")
        self.__genIncAndTypeDefs()
        self._genNameSpaceStart() 
        self.__genDeclarations()
        self._genNameSpaceEnd()
        self._genHeadFileEnd("COMMON")

    def __genIncAndTypeDefs(self):
        content_lines = """
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include "cpolaris.h"

typedef struct PolarisReadableMessage PolarisReadableMessage;
typedef struct PolarisRuntime PolarisRuntime;
typedef struct PolarisService PolarisService;
typedef struct PolarisSession PolarisSession;
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genDeclarations(self):
        self.__genBytesBuffer()
        self.__genNameIdMapper()
        self.__genMessageReader()
        self.__genMessageWriter()

        if not ir_parser.kDeclarationsOrder in self._ir_dict.keys():
            return

        for order_item in self._ir_dict[ir_parser.kDeclarationsOrder]:
            if order_item[ir_parser.kCategory] == ir_parser.kEnum:
                self.__genEnumDeclaration(order_item[ir_parser.kName])
            elif order_item[ir_parser.kCategory] == ir_parser.kStruct:
                self.__genStructDeclaration(order_item[ir_parser.kName])
            elif order_item[ir_parser.kCategory] == ir_parser.kUnion:
                self.__genUnionDeclaration(order_item[ir_parser.kName])

        self.__genDataWrapperStructs()

    def __genBytesBuffer(self):
        content_lines = """
struct BytesBuffer {
    std::vector<uint8_t> data;
};
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genEnumDeclaration(self, name):
        for item in self._ir_dict[ir_parser.kEnumDeclarations]:
            if not item[ir_parser.kName] == name:
                continue
            
            member_str = ""
            for member_item in item[ir_parser.kMembers]:
                member_name = member_item[ir_parser.kName]

                if ir_parser.kValue in member_item.keys():
                    member_str += """\n    %s = %s,""" % (member_name, member_item[ir_parser.kValue])
                else:
                    member_str += """\n    %s,""" % (member_name)

            content_lines = """
enum class %s {%s
};
""" %(name, member_str)
            utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genStructDeclaration(self, name):
        for item in self._ir_dict[ir_parser.kStructDeclarations]:
            if not item[ir_parser.kName] == name:
                continue

            member_str = ""
            for member_item in item[ir_parser.kMembers]:
                member_name = member_item[ir_parser.kName]
                member_type = self._typeConvert(member_item[ir_parser.kType])
                member_str += "    %s %s;\n" % (member_type, member_name)

            content_lines = """
struct %s final {
%s
    bool Deserialize(PolarisReadableMessage* message);
    void Serialize(PolarisWritableMessage* message) const;
};
""" %(name, member_str)
            utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genUnionDeclaration(self, name):
        for item in self._ir_dict[ir_parser.kUnionDeclarations]:
            if not item[ir_parser.kName] == name:
                continue

            member_str = ""
            member_name_list = []
            case_value_list = []
            member_type_list = []
            for member_item in item[ir_parser.kMembers]:
                member_name_list.append(member_item[ir_parser.kName])
                case_value_list.append(member_item[ir_parser.kCaseValue])
                member_type_list.append(self._typeConvert(member_item[ir_parser.kType]))

        # create tag content
        tag_item = ""
        for i in range(len(member_name_list)):
            tag_item += """\n        TYPE_%d = %d,""" % (i+1, i+1)
        tag_item += """\n        TYPE_RESERVED = %d""" % (len(member_name_list)+1)

        tag_str = """
    enum Tag : uint32_t {%s
    };
""" % (tag_item)

        # create constructor content
        constructor_str = "   Obj() = default;\n"
        for i in range(len(member_name_list)):
            constructor_str += """
    explicit %s(const %s& value)
            : tag_(TYPE_%d), %s(value) {}
""" % (name, member_type_list[i], i+1, member_name_list[i])

        # create SetValue content
        setvalue_str = ""
        for i in range(len(member_name_list)):
            setvalue_str += """
    void SetValue(const %s& value)
    {
        tag_ = TYPE_%d;
        %s = value;
    }
""" % (member_type_list[i], i+1, member_name_list[i])

        # create GetValue content
        getvalue_str = ""
        for i in range(len(member_name_list)):
            getvalue_str += """
    bool GetValue(%s* value) const
    {
        if (value == nullptr) {
            return false;
        }

        if (tag_ != TYPE_%d) {
            return false;
        }

        *value = %s;
        return true;
    }
""" % (member_type_list[i], i+1, member_name_list[i])

        # create variable content
        variable_str = ""
        for i in range(len(member_name_list)):
            variable_str += """
    %s %s;""" % (member_type_list[i], member_name_list[i])

        full_str = """
class %s final
{
public:
%s
%s
    Tag GetTag() const
    {
        return tag_;
    }
%s
%s
    void Serialize(PolarisWritableMessage* message) const;
    bool Deserialize(PolarisReadableMessage* message);

private:
    Tag tag_ = Tag::TYPE_RESERVED;%s
};
        """ % (name, tag_str, constructor_str, setvalue_str, getvalue_str, variable_str)
        utils.Utils.writeGenFile(self._path, self._file, full_str)


    def __genNameIdMapper(self):
        content_lines = """
class NameIdMapper
{
public:
    bool FindId(const std::string& name, uint16_t* id) const
    {
        if (id == nullptr) {
            return false;
        }

        auto iter = name_id_map_.find(name);

        if (iter == name_id_map_.end()) {
            return false;
        }

        *id = iter->second;
        return true;
    }

    bool FindName(const uint16_t id, const char** name, uint32_t* size) const
    {
        if (name == nullptr || size == nullptr) {
            return false;
        }

        auto iter = id_name_map_.find(id);

        if (iter == id_name_map_.end()) {
            return false;
        }

        *name = iter->second.c_str();
        *size = iter->second.size();
        return true;
    }

    void InsertNameId(const std::string& name, uint16_t id)
    {
        name_id_map_.emplace(name, id);
    }

    void InsertIdName(uint16_t id, const std::string& name)
    {
        id_name_map_.emplace(id, name);
    }

private:
    std::unordered_map<std::string, uint16_t> name_id_map_;
    std::unordered_map<uint16_t, std::string> id_name_map_;
};
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genMessageReader(self):
        content_lines = """
class MessageReader
{
public:
    explicit MessageReader(PolarisReadableMessage* message)
        : message_(message) {}
    bool Read(bool* value);

    bool Read(int8_t* value);

    bool Read(int16_t* value);

    bool Read(int32_t* value);

    bool Read(int64_t* value);

    bool Read(uint8_t* value);

    bool Read(uint16_t* value);

    bool Read(uint32_t* value);

    bool Read(uint64_t* value);

    bool Read(float* value);

    bool Read(double* value);

    bool Read(std::string* value);

    bool Read(BytesBuffer* value);

    template<typename ValueType>
    bool Read(std::vector<ValueType>* value)
    {
        if (value == nullptr) {
            return false;
        }

        int32_t size = 0;
        bool flag = message_->read_vector_begin(message_, &size);

        if(!flag) {
            return flag;
        }

        uint32_t count = (uint32_t)size;

        while (count > 0) {
            count--;
            ValueType member;

            if (!Read(&member)) {
                break;
            }

            value->push_back(std::move(member));
        }

        message_->read_vector_end(message_);

        return true;
    }

    template<typename ValueType, std::size_t size>
    bool Read(std::array<ValueType, size>* value)
    {
        if (value == nullptr) {
            return false;
        }

        bool flag = message_->read_array_begin(message_);

        if(!flag) {
            return flag;
        }

        for(std::size_t index = 0; index < size; index++) {
            ValueType member;
            Read(&member);
            (*value)[index] = std::move(member);
        }

        message_->read_array_end(message_);

        return true;
    }

    template<typename T>
    bool Read(T* value)
    {
        if (value == nullptr) {
            return false;
        }

        return value->Deserialize(message_);
    }

private:
    PolarisReadableMessage* message_;
};
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genMessageWriter(self):
        content_lines = """
class MessageWriter
{
public:
    explicit MessageWriter(PolarisWritableMessage* message)
        : message_(message) {}

    void Write(const bool& value);

    void Write(const int8_t& value);

    void Write(const int16_t& value);

    void Write(const int32_t& value);

    void Write(const int64_t& value);

    void Write(const uint8_t& value);

    void Write(const uint16_t& value);

    void Write(const uint32_t& value);

    void Write(const uint64_t& value);

    void Write(const float& value);

    void Write(const double& value);

    void Write(const std::string& value);

    void Write(const BytesBuffer& value);

    template<typename ValueType>
    void Write(const std::vector<ValueType>& value)
    {
        message_->write_vector_begin(message_, value.size());

        for(const ValueType& item : value) {
            Write(item);
        }

        message_->write_vector_end(message_);
    }

    template<typename ValueType, std::size_t size>
    void Write(const std::array<ValueType, size>& value)
    {
        message_->write_array_begin(message_);

        for(std::size_t index = 0; index < size; index++) {
            Write(value[index]);
        }

        message_->write_array_end(message_);
    }

    template<typename T>
    void Write(const T& value)
    {
        value.Serialize(message_);
    }

private:
    PolarisWritableMessage* message_;
};
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genDataWrapperStructs(self):
        if not ir_parser.kInterfaceDeclarations in self._ir_dict.keys():
            return

        for interface_item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            # for method data
            if ir_parser.kMethodList in interface_item.keys():
                for method_item in interface_item[ir_parser.kMethodList]:
                    # for request
                    if ir_parser.kMethodParameter in method_item.keys(): 
                        self.__genDataWrapperStructItem(interface_item[ir_parser.kName],
                                                method_item[ir_parser.kMethodName],
                                                method_item[ir_parser.kMethodParameter], "request")
                    # for response
                    if ir_parser.kMethodReturn in method_item.keys(): 
                        self.__genDataWrapperStructItem(interface_item[ir_parser.kName],
                                                method_item[ir_parser.kMethodName],
                                                method_item[ir_parser.kMethodReturn], "response")

            # for event data
            if ir_parser.kEventList in interface_item.keys(): 
                for event_item in interface_item[ir_parser.kEventList]:
                    # for notify
                    if ir_parser.kMembers in event_item.keys(): 
                        self.__genDataWrapperStructItem(interface_item[ir_parser.kName],
                                                        event_item[ir_parser.kEventName],
                                                        event_item[ir_parser.kMembers], "notify")

    def __genDataWrapperStructItem(self, interface_name, function_name, members, type):
        type_name = "Req"
        if type == "response":
            type_name = "Resp"
        elif type == "notify":
            type_name = "Notify"

        args_str = ""
        i = 1
        for arg in members:
            arg_name = ""

            if ir_parser.kName in arg.keys():
                arg_name = arg[ir_parser.kName]
            else:
                # if arg name is not provided, use arg name as function_arg_X
                arg_name = "%s_arg_%d" %(function_name, i)

            arg_type = self._typeConvert(arg[ir_parser.kType])
            if arg_type == "void":
                return
            args_str +="    const %s& %s;\n" %(arg_type, arg_name)
            i += 1

        content_lines = """
struct %s_%s_%s {
%s
};
""" %(interface_name, function_name, type_name, args_str)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)





