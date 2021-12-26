import logging
from string import Template
import sys
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
from . import cpp_gen_protocol


class CppServiceHeaderGenerator(cpp_gen_protocol.CppGeneratorProtocol):
    def __init__(self, path, base_name, ir_dict):
        super(CppServiceHeaderGenerator, self).__init__(path, base_name, base_name + "Service.h", ir_dict)

    def gen(self):
        print("start to gen service header")
        self._genHeadFileStart("SERVICE")
        self.__genIncAndTypeDefs()
        self._genNameSpaceStart() 
        self.__genDeclarations()
        self._genNameSpaceEnd()
        self._genHeadFileEnd("SERVICE")

    def __genIncAndTypeDefs(self):
        content_lines = """
#include <functional>
#include "%sCommon.h"
""" %(self._base_name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genDeclarations(self):
        self.__genSessionAndFunc()
        self.__genForwardDecl()       

        if not ir_parser.kDeclarationsOrder in self._ir_dict.keys():
            return

        for order_item in self._ir_dict[ir_parser.kDeclarationsOrder]:
            if order_item[ir_parser.kCategory] == ir_parser.kInterface:
                self.__genInterfaceDecl(order_item[ir_parser.kName])

    def __genSessionAndFunc(self):
        content_lines = """
struct SessionContext final {    
    // Transport Channel ID    
    uint32_t channel;

    // Request token
    std::string token;

    // Client identifier
    std::string client_identifier;

    /**
    * Permission check result
    * @note at service side, polaris:: runtime checks the permission of client's
    * request and set the check result in 'has_permission' field.
    */
    bool has_permission = false;
};

using SessionHandler =
    std::function<void(const SessionContext& session, bool active)>;

using CommunicationHandler =
    std::function<void(bool available)>;
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genForwardDecl(self):
        member_str = ""
        for item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            member_str += """
class %sService;
class %sServiceImpl;
""" % (item[ir_parser.kName], item[ir_parser.kName])
        utils.Utils.writeGenFile(self._path, self._file, member_str)

    def __genInterfaceDecl(self, name):
        for item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            if not item[ir_parser.kName] == name:
                continue

            self.__genServiceDecl(item)
            self.__genAbstractServiceDecl(item)

    def __genServiceDecl(self, info):
        methods_str = ""
        interface_name = info[ir_parser.kName]
        for method_item in info[ir_parser.kMethodList]:
            method_name = method_item[ir_parser.kMethodName]
            # idl may support multi return value in future, bug currently only support one
            if ir_parser.kMethodReturn in method_item.keys():
                return_type = self._typeConvert(method_item[ir_parser.kMethodReturn][0][ir_parser.kType])

            args_str = ""
            if ir_parser.kMethodParameter in method_item.keys():
                args_str = self._getArgListStr(method_name, method_item[ir_parser.kMethodParameter],
                                                            "    const ", "&", ",\n")

            # for using XxxReplyer declaration
            if not return_type == "void":
                methods_str += """
    using %sReplyer = std::function<void (const %s& data)>;
""" % (method_name, return_type)

            # for using XxxHandler declaration
            methods_str += """
    using %sHandler = std::function<void(const SessionContext& ctx""" % (method_name)

            if not args_str == "":
                methods_str += """,
                            %s""" % (args_str)

            if not return_type == "void":
                methods_str += """,
                            const %sReplyer& replyer""" % (method_name)
            methods_str += ")>;"

            # for RegisterXxxHandler declaration
            methods_str += """
    void Register%sHandler(const %sHandler& handler);
""" % (method_name, method_name)

        events_str = ""
        for event_item in info[ir_parser.kEventList]:
            event_name = event_item[ir_parser.kEventName]
            args_str = self._getArgListStr(event_name, event_item[ir_parser.kMembers], "\n        const ", "&", ",")
            events_str += """
    void Notify%s(%s);
""" % (event_name, args_str)

        content_lines = """
class %sService final
{
public:
    %sService();

    %sService(const %sService&) = delete;

    %sService& operator=(const %sService&) = delete;

    bool Start();

    void Stop();
%s
%s
    void RegisterSessionHandler(const SessionHandler& handler);

    void RegisterCommunicationHandler(const CommunicationHandler& handler);

private:
    std::shared_ptr<%sServiceImpl> impl_;
};
""" %(interface_name, interface_name, interface_name, interface_name, interface_name,
        interface_name, methods_str, events_str, interface_name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)             

    def __genAbstractServiceDecl(self, info):
        replyer_str, handler_str, v_method_api = self.__getAbstractServiceMethod(info)
        temp = Template("""
class ${interface}AbstractService
{
public:
${replyer}

public:
    ${interface}AbstractService();

    virtual ~${interface}AbstractService();

    ${interface}AbstractService(const ${interface}AbstractService&) = delete;

    ${interface}AbstractService& operator=(const ${interface}AbstractService&) = delete;

    bool Start();

    void Stop();
${event}

private:
    virtual void handleSession(const SessionContext& session, bool active) {}
    virtual void handleCommStatus(bool available) {}
${v_method_api}

private:
    static void ${interface}RequestHandler(void* user_data,
                                        PolarisReadableMessage* message);

    static void ${interface}SessionHandler(void* user_data,
                                        const PolarisSession* session, bool active);

    static void ${interface}CommHandler(void* user_data,
                                     bool available);

    void onRequest(PolarisReadableMessage* request);

${method_handler}
    void initNameIdMapping();
private:
    PolarisRuntime* runtime_ = nullptr;
    PolarisService* service_ = nullptr;
    std::shared_ptr<NameIdMapper> name_id_map_;
};
""")

        content = temp.substitute(interface = info[ir_parser.kName],
                                    replyer = replyer_str,
                                    method_handler = handler_str,
                                    v_method_api = v_method_api,
                                    event = self.__getAbstractServiceEvent(info))

        utils.Utils.writeGenFile(self._path, self._file, content)

    def __getAbstractServiceMethod(self, info):
        replyer = ""
        handler = ""
        virtual_api = ""

        if not ir_parser.kMethodList in info.keys():
            return "", "", ""

        for method_item in info[ir_parser.kMethodList]:
            method_name = method_item[ir_parser.kMethodName]

            handler += """
    void on%s(PolarisReadableMessage* request, const std::string& permission);
""" %(method_name)
            in_args_str = ""
            if ir_parser.kMethodParameter in method_item.keys():
                in_args_str =  self._getArgListStr(method_name, method_item[ir_parser.kMethodParameter],
                                                                "\n            const ", "&", ",")
                if not in_args_str == "":
                    in_args_str = "," + in_args_str
            if ir_parser.kMethodReturn in method_item.keys()\
                and not self._isArgsListEmpty(method_item[ir_parser.kMethodReturn]):
                out_args_str = self._getArgListStr(method_name, method_item[ir_parser.kMethodReturn],
                                                            "\n            const ", "&", ",")
                replyer += """
    using %sReplyer = std::function<void(%s)>; 
""" %(method_name, out_args_str)

                virtual_api += """
    virtual void handle%s(
            const SessionContext& ctx%s,
            const %sReplyer& replyer) {}
""" %(method_name, in_args_str, method_name)
            else:
                virtual_api += """
    virtual void handle%s(
            const SessionContext& ctx%s) {}
""" %(method_name, in_args_str)

        return replyer, handler, virtual_api

    def __getAbstractServiceEvent(self, info):
        result = ""
        if not ir_parser.kEventList in info.keys():
            return ""

        for event_item in info[ir_parser.kEventList]:
            args_str = ""
            if ir_parser.kMembers in event_item.keys():
                event_name = event_item[ir_parser.kEventName]
                args_str = self._getArgListStr(event_name, event_item[ir_parser.kMembers],
                                                            "\n            const ", "&", ",")
                result += """
    void Notify%s(%s); 
""" %(event_name, args_str)

        return result
  
