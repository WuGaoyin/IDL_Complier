import logging
from string import Template
import sys
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
from . import cpp_gen_protocol


class CppProxyHeaderGenerator(cpp_gen_protocol.CppGeneratorProtocol):
    def __init__(self, path, base_name, ir_dict):
        super(CppProxyHeaderGenerator, self).__init__(path, base_name, base_name + "Proxy.h", ir_dict)

    def gen(self):
        print("start to gen proxy header")
        self._genHeadFileStart("PROXY")
        self.__genIncAndTypeDefs()
        self._genNameSpaceStart() 
        self.__genDeclarations()
        self._genNameSpaceEnd()
        self._genHeadFileEnd("PROXY")

    def __genIncAndTypeDefs(self):
        content_lines = """
#include <functional>
#include "%sCommon.h"
""" %(self._base_name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genDeclarations(self):
        self.__genStableDecls()
        self.__genForwardDecls()       

        if not ir_parser.kDeclarationsOrder in self._ir_dict.keys():
            return

        for order_item in self._ir_dict[ir_parser.kDeclarationsOrder]:
            if order_item[ir_parser.kCategory] == ir_parser.kInterface:
                self.__genInterfaceDecl(order_item[ir_parser.kName])

    def __genStableDecls(self):
        content_lines = """
enum class ErrorCode {
    SUCCESS = 0,
    NO_SERVICE,
    REQUEST_FAILED,
    TIME_OUT,
    PARAM_INVALID,
    INTERNAL_ERROR,
};

enum class WaitResult {
    kSuccess = 0,
    kTimeout,
    kFailed
};

using ServiceStatusCallback = std::function<void(bool available)>;
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genForwardDecls(self):
        member_str = ""
        for item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            member_str += """
class %sProxy;
class %sProxyImpl;
""" % (item[ir_parser.kName], item[ir_parser.kName])
        utils.Utils.writeGenFile(self._path, self._file, member_str)


    def __genInterfaceDecl(self, name):
        for item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            if not item[ir_parser.kName] == name:
                continue

            self.__genProxyInterfaceDecl(item)

    def __genProxyInterfaceDecl(self, interface_info):
        temp = Template("""
class ${interface}Proxy final
{
public:
    explicit ${interface}Proxy(const std::string& app_name);

    ${interface}Proxy(const ${interface}Proxy&) = delete;

    ${interface}Proxy& operator=(const ${interface}Proxy&) = delete;

    void WatchServiceStatus(const ServiceStatusCallback& callback);

    bool IsServiceActive();

    WaitResult WaitService(int32_t timeout);

    void Unwatch(const std::string& event_name);

${method_str}
${event_str}

private:
    std::shared_ptr<${interface}ProxyImpl> impl_;
};
""")     

        content = temp.substitute(interface = interface_info[ir_parser.kName],
                        method_str = self.__getProxyMethodsStr(interface_info),
                        event_str = self.__getProxyEventsStr(interface_info))
        utils.Utils.writeGenFile(self._path, self._file, content)

    def __getProxyMethodsStr(self, interface_info):
        result = ""        

        for method_item in interface_info[ir_parser.kMethodList]:
            in_args_str = ""
            method_name = method_item[ir_parser.kMethodName]

            if ir_parser.kMethodParameter in method_item.keys():
                in_args_str = self._getArgListStr("in", method_item[ir_parser.kMethodParameter],
                                                            "\n            const ", "&", ",")
            if ir_parser.kMethodReturn in method_item.keys():      
                out_args_str = self._getArgListStr("in", method_item[ir_parser.kMethodReturn],
                                                            "\n            ", "*", ",")
            if out_args_str == "":
                temp = Template("""
    ErrorCode ${method}(${in_args});
""")
            else:
                if not in_args_str == "":
                    in_args_str += ","
                temp = Template("""
    using ${method}Callback = std::function<void(
            ErrorCode,${out_args}
            )>;

    ErrorCode ${method}Sync(${in_args}${out_args},
            int timeout_msec);

    void ${method}Async(${in_args}
            const ${method}Callback& callback);
""")
            result += temp.substitute(method = method_name, in_args = in_args_str, 
                                    out_args = out_args_str)
        return result

    def __getProxyEventsStr(self, interface_info):
        result = ""        

        for event_item in interface_info[ir_parser.kEventList]:
            in_args_str = ""
            event_name = event_item[ir_parser.kEventName]

            if ir_parser.kMembers in event_item.keys():
                in_args_str = self._getArgListStr("in", event_item[ir_parser.kMembers],
                                                            "const ", "&", ",")
            temp = Template("""
    using ${event}Callback =
        std::function<void(${in_args})>;

    void On${event}(const ${event}Callback& callback);

    void Off${event}();
""")
            result += temp.substitute(event = event_name, in_args = in_args_str)
        return result

