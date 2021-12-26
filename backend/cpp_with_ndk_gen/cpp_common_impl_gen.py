import logging
from string import Template
import sys
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
from . import cpp_gen_protocol


class CppCommonImplGenerator(cpp_gen_protocol.CppGeneratorProtocol):
    def __init__(self, path, base_name, ir_dict):
        super(CppCommonImplGenerator, self).__init__(path, base_name, base_name + "Common.cpp", ir_dict)

    def gen(self):
        print("start to gen common impl")
        self.__genIncAndTypeDefs()
        self._genNameSpaceStart() 
        self.__genImplementations()
        self._genNameSpaceEnd()

    def __genIncAndTypeDefs(self):
        content_lines = '''#include "%sCommon.h"\n''' % (self._base_name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genImplementations(self):
        if not ir_parser.kDeclarationsOrder in self._ir_dict.keys():
            return

        for order_item in self._ir_dict[ir_parser.kDeclarationsOrder]:
            if order_item[ir_parser.kCategory] == ir_parser.kStruct:
                self.__genStructImplementation(order_item[ir_parser.kName])
            elif order_item[ir_parser.kCategory] == ir_parser.kUnion:
                self.__genUnionImplementation(order_item[ir_parser.kName])

        self.__genMessageReader()
        self.__genMessageWriter()     

    
    def __genStructImplementation(self, name):
        for item in self._ir_dict[ir_parser.kStructDeclarations]:
            if item[ir_parser.kName] == name:
                read_member_str = ""
                write_member_str = ""
                for member_item in item[ir_parser.kMembers]:
                    member_name = member_item[ir_parser.kName]
                    member_type = self._typeConvert(member_item[ir_parser.kType])
                    read_member_str += "    reader.Read(&(this->%s));\n" % (member_name)
                    write_member_str += "    writer.Write(this->%s);\n" % (member_name)

                content_lines = """
void %s::Serialize(PolarisWritableMessage* message) const
{
    if (message == nullptr) {
        return;
    }

    MessageWriter writer(message);
    message->write_struct_begin(message);
%s
    message->write_struct_end(message);
}

bool %s::Deserialize(PolarisReadableMessage* message)
{
    if (message == nullptr) {
        return false;
    }

    MessageReader reader(message);

    if(!message->read_struct_begin(message)) {
        return false;
    }
%s
    message->read_struct_end(message);
    return true;
}
""" %(name, write_member_str, name, read_member_str)
                utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genUnionImplementation(self, name):
        for item in self._ir_dict[ir_parser.kUnionDeclarations]:
            if item[ir_parser.kName] == name:
                read_member_str = ""
                write_member_str = ""
                member_name_list = []
 
                for member_item in item[ir_parser.kMembers]:
                    member_name_list.append(member_item[ir_parser.kName])

        # create write member content
        write_member_str = ""
        for i in range(len(member_name_list)):
            write_member_str += """
    case Tag::TYPE_%d:
        writer.Write(this->%s);
        break;
        """ % (i+1, member_name_list[i])

        # create read member content
        read_member_str = ""
        for i in range(len(member_name_list)):
            read_member_str += """
    case Tag::TYPE_%d:
        reader.Read(&(this->%s));
        break;
        """ % (i+1, member_name_list[i])

        full_str = """
void %s::Serialize(PolarisWritableMessage* message) const
{
    if (message == nullptr) {
        return;
    }

    MessageWriter writer(message);
    message->write_union_begin(message, tag_);

    switch (tag_) {
%s
    default:
        break;
    }

    message->write_union_end(message);
}

bool %s::Deserialize(PolarisReadableMessage* message)
{
    if (message == nullptr) {
        return false;
    }

    MessageReader reader(message);
    bool flag = message->read_union_begin(message, (uint32_t*)&tag_);

    if(!flag) {
        return false;
    }

    switch (tag_) {
%s
    default:
        break;
    }

    message->read_union_end(message);
    return true;
}
        """ % (name, write_member_str, name, read_member_str)
        utils.Utils.writeGenFile(self._path, self._file, full_str) 

    def __genMessageReader(self):
        content_lines = """
bool MessageReader::Read(bool* value)
{
    uint8_t result;

    if(!message_->read_uint8(message_, &result)) {
        return false;
    }

    *value = result > 0 ? true : false;
    return true;
}

bool MessageReader::Read(int8_t* value)
{
    return message_->read_int8(message_, value);
}

bool MessageReader::Read(int16_t* value)
{
    return message_->read_int16(message_, value);
}

bool MessageReader::Read(int32_t* value)
{
    return message_->read_int32(message_, value);
}

bool MessageReader::Read(int64_t* value)
{
    return message_->read_int64(message_, value);
}

bool MessageReader::Read(uint8_t* value)
{
    return message_->read_uint8(message_, value);
}

bool MessageReader::Read(uint16_t* value)
{
    return message_->read_uint16(message_, value);
}

bool MessageReader::Read(uint32_t* value)
{
    return message_->read_uint32(message_, value);
}

bool MessageReader::Read(uint64_t* value)
{
    return message_->read_uint64(message_, value);
}

bool MessageReader::Read(float* value)
{
    return message_->read_float(message_, value);
}

bool MessageReader::Read(double* value)
{
    return message_->read_double(message_, value);
}

bool MessageReader::Read(std::string* value)
{
    const char* str = nullptr;
    uint32_t size = 0;

    if(!message_->read_string(message_, &str, &size)) {
        return false;
    }

    *value = str;
    delete [] str;
    return true;
}

bool MessageReader::Read(BytesBuffer* value)
{
    int8_t* buffer = nullptr;
    uint32_t size = 0;

    if(!message_->read_byte_buffer(message_, &buffer, &size)) {
        return false;
    }

    uint8_t* temp = reinterpret_cast<uint8_t*>(buffer);
    value->data.assign(temp, temp + size);
    delete [] buffer;
    return true;
}
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genMessageWriter(self):
        content_lines = """
void MessageWriter::Write(const bool& value)
{
    message_->write_uint8(message_, value);
}

void MessageWriter::Write(const int8_t& value)
{
    message_->write_int8(message_, value);
}

void MessageWriter::Write(const int16_t& value)
{
    message_->write_int16(message_, value);
}

void MessageWriter::Write(const int32_t& value)
{
    message_->write_int32(message_, value);
}

void MessageWriter::Write(const int64_t& value)
{
    message_->write_int64(message_, value);
}

void MessageWriter::Write(const uint8_t& value)
{
    message_->write_uint8(message_, value);
}

void MessageWriter::Write(const uint16_t& value)
{
    message_->write_uint16(message_, value);
}

void MessageWriter::Write(const uint32_t& value)
{
    message_->write_uint32(message_, value);
}

void MessageWriter::Write(const uint64_t& value)
{
    message_->write_uint64(message_, value);
}

void MessageWriter::Write(const float& value)
{
    message_->write_float(message_, value);
}

void MessageWriter::Write(const double& value)
{
    message_->write_double(message_, value);
}

void MessageWriter::Write(const std::string& value)
{
    message_->write_string(message_, value.c_str());
}

void MessageWriter::Write(const BytesBuffer& value)
{
    message_->write_byte_buffer(message_, value.data.data(), value.data.size());
}
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)






