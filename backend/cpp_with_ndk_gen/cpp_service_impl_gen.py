import logging
from string import Template
import sys
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
from . import cpp_gen_protocol


class CppServiceImplGenerator(cpp_gen_protocol.CppGeneratorProtocol):
    def __init__(self, path, base_name, ir_dict):
        super(CppServiceImplGenerator, self).__init__(path, base_name, base_name + "Service.cpp", ir_dict)

    def gen(self):
        print("start to gen service impl")
        self.__genIncAndTypeDefs()
        self._genNameSpaceStart()
        self.__genNameIdMapper()
        self.__genImpl()
        self._genNameSpaceEnd()

    def __genIncAndTypeDefs(self):
        content_lines = '''#include "%sService.h"\n''' % (self._base_name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genNameIdMapper(self, ):
        content_lines = """
using std::shared_ptr;
using std::string;

static bool NameToId(void* user_data, const char* name, uint16_t* id)
{
    NameIdMapper* object =
        reinterpret_cast<NameIdMapper*>(user_data);

    if (object == nullptr) {
        return false;
    }

    return object->FindId(name, id);
}

static bool IdToName(void* user_data, uint16_t id, const char** name, uint32_t* size)
{
    NameIdMapper* object =
        reinterpret_cast<NameIdMapper*>(user_data);

    if (object == nullptr) {
        return false;
    }

    return object->FindName(id, name, size);
}
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines) 

    def __genImpl(self):
        if not ir_parser.kDeclarationsOrder in self._ir_dict.keys():
            return

        for order_item in self._ir_dict[ir_parser.kDeclarationsOrder]:
            if order_item[ir_parser.kCategory] == ir_parser.kInterface:
                self.__genInterfaceImpl(order_item[ir_parser.kName])
    
    def __genInterfaceImpl(self, name):
        for item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            if not item[ir_parser.kName] == name:
                continue
            self.__genStaticHandlerDecl(name)
            self.__genCodec(item)
            self.__genImplClass(item)
            self.__genStaticHandlerImpl(name)
            self.__genServiceClass(item)
            self.__genAbstractServiceImpl(item)

    def __genStaticHandlerDecl(self, name):
        content_lines = """
static void %sRequestHandler(
            void* user_data, PolarisReadableMessage* message);
static void %sSessionHandler(
            void* user_data, const PolarisSession* session, bool active);
static void %sCommHandler(
            void* user_data, bool available);
""" %(name, name, name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genStaticHandlerImpl(self, name):
        temp = Template("""
static void ${interface_name}RequestHandler(
    void* user_data, PolarisReadableMessage* message)
{
    ${interface_name}ServiceImpl* impl =
        reinterpret_cast<${interface_name}ServiceImpl*>(user_data);

    if (impl == nullptr) {
        return;
    }

    impl->OnRequest(message);
}

static void ${interface_name}SessionHandler(
    void* user_data, const PolarisSession* session, bool active)
{
    ${interface_name}ServiceImpl* service =
        reinterpret_cast<${interface_name}ServiceImpl*>(user_data);

    if (service == nullptr) {
        return;
    }

    bool status = active > 0 ? true : false;
    SessionContext ctx;
    ctx.channel = session->channel;
    ctx.token = session->token;
    ctx.client_identifier = session->client_identifier;
    service->OnSession(ctx, status);
}

static void ${interface_name}CommHandler(
    void* user_data, bool available)
{
    ${interface_name}ServiceImpl* service =
        reinterpret_cast<${interface_name}ServiceImpl*>(user_data);

    if (service == nullptr) {
        return;
    }

    bool status = available > 0 ? true : false;
    service->OnCommStatus(status);
}

""")
        content_lines = temp.substitute(interface_name = name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genCodec(self, interface_info):
        interface_name = interface_info[ir_parser.kName]
        method_str = self.__getCodecMethodStr(interface_info)
        event_str = self.__getCodecEventStr(interface_info)
        content_lines = """
class %sCodec
{
public:
%s
%s
};
""" %(interface_name, method_str, event_str)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __getCodecMethodStr(self, interface_info):
        result = ""
        if not ir_parser.kMethodList in interface_info.keys():
            return result

        for method_item in interface_info[ir_parser.kMethodList]:
            method_name = method_item[ir_parser.kMethodName]

            if not ir_parser.kMethodReturn in method_item.keys()\
                or self._isArgsListEmpty(method_item[ir_parser.kMethodReturn]):
                continue
            # idl may support multi return value in future, bug currently only support one
            temp = Template("""
    static void ${method}ReplyDecorator(
        void* user_data, PolarisWritableMessage* message)
    {
        ${interface}_${method}_Resp* argument = reinterpret_cast<${interface}_${method}_Resp*>(user_data);

        if (argument == nullptr) {
            return;
        }

        MessageWriter writer(message);
        message->serialize_begin(message, 1);
        writer.Write(argument->${method}_arg_1);
        message->serialize_end(message);
    }
""")
            result += temp.substitute(method=method_name, interface=interface_info[ir_parser.kName])
        return result

    def __getCodecEventStr(self, interface_info):
        result = ""
        if not ir_parser.kEventList in interface_info.keys():
            return result

        for event_item in interface_info[ir_parser.kEventList]:
            event_name = event_item[ir_parser.kEventName]
            event_data = ""
            if not ir_parser.kMembers in event_item.keys() \
                or self._isArgsListEmpty(event_item[ir_parser.kMembers]):
                continue
            
            for member in event_item[ir_parser.kMembers]:                    
                event_data += "        writer.Write(argument->%s);\n" %(member[ir_parser.kName])

            temp = Template("""
    static void ${event}NotifyDecorator(
        void* user_data, PolarisWritableMessage* message)
    {
        ${interface}_${event}_Notify* argument = reinterpret_cast<${interface}_${event}_Notify*>(user_data);

        if (argument == nullptr) {
            return;
        }

        MessageWriter writer(message);
        message->serialize_begin(message, ${num});
${event_data}
        message->serialize_end(message);

    }    
""")
            result += temp.substitute(event=event_name, interface=interface_info[ir_parser.kName], 
            event_data = event_data, num = len(event_item[ir_parser.kMembers]))
        return result

    def __genImplClass(self, interface_info):
        temp = Template("""
class ${interface_name}ServiceImpl final
{
public:
    ${interface_name}ServiceImpl()
    {
        runtime_ = PolarisCreateRuntime();
        name_id_map_ = std::make_shared<NameIdMapper>();

        if (runtime_ == nullptr || name_id_map_ == nullptr) {
            return;
        }

        initNameIdMapping();

        PolarisServiceIdentifier identifier;
        identifier.service_name = "${namespace}.${interface_name}";
        service_ = PolarisCreateService(runtime_, &identifier, POLARIS_CHANNEL_DDS,
                                       IdToName, &name_id_map_,
                                       NameToId,  &name_id_map_);
    }

    ~${interface_name}ServiceImpl()
    {
        if (runtime_ == nullptr) {
            return;
        }

        if (service_ != nullptr) {
            PolarisDestroyService(runtime_, service_);
            service_ = nullptr;
        }

        PolarisDestroyRuntime(runtime_);
        runtime_ = nullptr;
    }

    bool Start()
    {
        if (service_ == nullptr) {
            return false;
        }

        service_->start(service_, ${interface_name}RequestHandler, this,
                        ${interface_name}SessionHandler, this,
                        ${interface_name}CommHandler, this);

        return true;
    }

    void Stop()
    {
        if (service_ == nullptr) {
            return;
        }

        service_->stop(service_);
    }

    void OnSession(const SessionContext& session, bool active)
    {
        if (session_handler_ != nullptr) {
            session_handler_(session, active);
        }
    }

    void OnCommStatus(bool available)
    {
        if (communication_handler_ != nullptr) {
            communication_handler_(available);
        }
    }

    void RegisterSessionHandler(const SessionHandler& handler)
    {
        session_handler_ = handler;
    }

    void RegisterCommunicationHandler(const CommunicationHandler& handler)
    {
        communication_handler_ = handler;
    }
${method}
${event}

private:
    void initNameIdMapping()
    {
        std::vector<std::string> all_names = {${method_event_names}
                                             };

        for(size_t i = 0; i < all_names.size(); i++) {
            name_id_map_->InsertNameId(all_names[i], i);
            name_id_map_->InsertIdName(i, all_names[i]);
        }
    }

private:
    std::shared_ptr<NameIdMapper> name_id_map_;
    PolarisRuntime* runtime_ = nullptr;
    PolarisService* service_ = nullptr;

${member_variable_handler}
    SessionHandler session_handler_;
    CommunicationHandler communication_handler_;
};   
""")
        method_str, member_variable_handler_str = self.__getImplClassMethodStr(interface_info)
        content_lines = temp.substitute(namespace = self._full_name_space,
                                        interface_name=interface_info[ir_parser.kName],
                                        method = method_str,
                                        member_variable_handler = member_variable_handler_str,
                                        event = self.__getImplClassEventStr(interface_info),
                                        method_event_names = self._getMethodEventNamesStr(interface_info,
                                                            "\n                                    ", ","))
        utils.Utils.writeGenFile(self._path, self._file, content_lines)
        
    def __getImplClassMethodStr(self, interface_info):
        method_str = ""
        interface_name = interface_info[ir_parser.kName]
        request_str = ""
        register_str = ""
        handler_str = ""
        member_variable_handler_str = ""

        if ir_parser.kMethodList in interface_info.keys():
            for method in interface_info[ir_parser.kMethodList]:
                method_name = method[ir_parser.kMethodName]
                request_str += self.__getImplClassMethodReqStr(request_str=="", method_name)
                register_str += self.__getImplClassMethodRegStr(interface_name, method_name)
                handler_str += self.__getImplClassMethodHandlerStr(interface_name, method)
                member_variable_handler_str += "\n    %sService::%sHandler %s_handler_;" % (interface_name, method_name, method_name)
        method_str += """
    void OnRequest(PolarisReadableMessage* request)
    {
        std::string request_name = request->get_name(request);
%s
    }
%s
%s    
""" % (request_str, register_str, handler_str)
        return method_str, member_variable_handler_str

    def __getImplClassEventStr(self, interface_info):
        result = ""
    
        if ir_parser.kEventList in interface_info.keys():            
            no_type_in_args = ""
            for item in interface_info[ir_parser.kEventList]:
                in_args = ""
                event_name = item[ir_parser.kEventName]
                if ir_parser.kMembers in item \
                    and not self._isArgsListEmpty(item[ir_parser.kMembers]):
                    args_list = item[ir_parser.kMembers]
                    no_type_in_args = self._getNoTypeArgListStr("in", args_list, "", ",")
                    in_args = self._getArgListStr("in", args_list, "const ", "&", ",")
                    temp = Template("""
    void Notify${event}(${in_args})
    {
        if (service_ == nullptr) {
            return;
        }

        ${interface}_${event}_Notify argument = {${no_type_in_args}};
        service_->notify(service_, "${event}",
                        ${interface}Codec::${event}NotifyDecorator, &argument);
    }  
""")
                else:
                    temp = Template("""
    void Notify${event}(${in_args})
    {
        if (service_ == nullptr) {
            return;
        }

        service_->notify(service_, "${event}", nullptr, nullptr);
    }  
""") 
                result += temp.substitute(interface = interface_info[ir_parser.kName],
                                            event = event_name, in_args = in_args,
                                            no_type_in_args = no_type_in_args)
        return result

    def __getImplClassMethodReqStr(self, is_first, method_name):
        result = ""
        if is_first:
            result = """
        if (request_name == "%s") {
            std::string permission = "";
            Handle%s(request, permission);
        }
""" %(method_name, method_name)
        else:
            result = """
        else if (request_name == "%s") {
            std::string permission = "";
            Handle%s(request, permission);
        }
""" %(method_name, method_name)

        return result

    def __getImplClassMethodRegStr(self, interface_name, method_name):
        return """
    void Register%sHandler(const %sService::%sHandler& handler)
    {
        %s_handler_ = handler;
    }
""" % (method_name, interface_name, method_name, method_name)

    def __getImplClassMethodHandlerStr(self, interface_name, method_info):
        method_name = method_info[ir_parser.kMethodName]
        reader_str = ""
        in_args_str = ""

        if ir_parser.kMethodParameter in method_info.keys()\
            and not self._isArgsListEmpty(method_info[ir_parser.kMethodParameter]):
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = "," + self._getNoTypeArgListStr("in", args_list, "", ",")

            for arg in method_info[ir_parser.kMethodParameter]:
                arg_type = self._typeConvert(arg[ir_parser.kType])
                arg_name = arg[ir_parser.kName]
                if arg_type == "void":
                    reader_str = ""
                    break
                reader_str += """                
        %s %s;
        reader.Read(&%s);                
""" %(arg_type, arg_name, arg_name)

        reply_str = ""
        out_args_str = ""
        no_type_out_args_str = ""
        reply_handler_str = ""
        if ir_parser.kMethodReturn in method_info.keys() \
            and not self._isArgsListEmpty(method_info[ir_parser.kMethodReturn]):
            args_list = method_info[ir_parser.kMethodReturn]
            no_type_out_args_str = self._getNoTypeArgListStr("out", args_list, "", ",")
            out_args_str = self._getArgListStr("out", args_list, "const ", "&", ",")            
            reply_handler_str = "," + "handler"
            temp = Template("""
        auto handler = [this, cloned_request](${out_args})
        {
            ${interface}_${method}_Resp argument = {${no_type_out_args}};
            service_->reply(service_, cloned_request,
                            ${interface}Codec::${method}ReplyDecorator, &argument);
            PolarisDestroySyncReplyMessage(cloned_request);
        };
""")
            reply_str += temp.substitute(method=method_name, interface=interface_name, out_args = out_args_str,
                                            no_type_out_args = no_type_out_args_str)

        temp = Template("""
    void Handle${method}(PolarisReadableMessage* request, const std::string& permission)
    {
        if (${method}_handler_ == nullptr) {
            return;
        }

        MessageReader reader(request);
${reader_content}

        uint32_t channel = 0;
        request->get_channel(request, &channel);
        std::string token = request->get_token(request);
        bool check_result = true;

        if(!permission.empty()) {
            check_result = service_->verify_permission(service_, channel,
                           token.c_str(), permission.c_str());
        }

        SessionContext ctx;
        ctx.channel = channel;
        ctx.token = token;
        ctx.has_permission = check_result;
        PolarisReadableMessage* cloned_request = request->clone(request);
${reply_content}
        ${method}_handler_(ctx${in_args}${reply_handler});
    }
""")
        return temp.substitute(method=method_name, interface=interface_name, 
                                in_args = in_args_str, reader_content = reader_str,
                                reply_handler = reply_handler_str,
                                reply_content = reply_str)

    def __genServiceClass(self, interface_info):
        temp = Template("""
${interface}Service::${interface}Service()
    : impl_(std::make_shared<${interface}ServiceImpl>()) {}
void ${interface}Service::RegisterSessionHandler(const SessionHandler& handler)
{
    if (impl_ != nullptr) {
        impl_->RegisterSessionHandler(handler);
    }
}

void ${interface}Service::RegisterCommunicationHandler(const CommunicationHandler& handler)
{
    if (impl_ != nullptr) {
        impl_->RegisterCommunicationHandler(handler);
    }
}

bool ${interface}Service::Start()
{
    if (impl_ != nullptr) {
        return impl_->Start();
    }

    return false;
}

void ${interface}Service::Stop()
{
    if (impl_ != nullptr) {
        impl_->Stop();
    }
}

${register_handler}
${notify}
""")
        content = temp.substitute(interface = interface_info[ir_parser.kName],
                                    register_handler = self.__getServiceClassRegHandlerStr(interface_info),
                                    notify = self.__getServiceClassNotifyStr(interface_info))
        utils.Utils.writeGenFile(self._path, self._file, content)

    def __genAbstractServiceImpl(self, interface_info):
        temp = Template("""
${interface}AbstractService::${interface}AbstractService()
{
    runtime_ = PolarisCreateRuntime();
    name_id_map_ = std::make_shared<NameIdMapper>();

    if (runtime_ == nullptr || name_id_map_ == nullptr) {
        return;
    }

    initNameIdMapping();

    PolarisServiceIdentifier identifier;
    identifier.service_name = "${namespace}.${interface}";
    service_ = PolarisCreateService(runtime_, &identifier, POLARIS_CHANNEL_DDS,
                                   IdToName, &name_id_map_,
                                   NameToId,  &name_id_map_);
}

${interface}AbstractService::~${interface}AbstractService()
{
    if (runtime_ == nullptr) {
        return;
    }

    if (service_ != nullptr) {
        PolarisDestroyService(runtime_, service_);
        service_ = nullptr;
    }

    PolarisDestroyRuntime(runtime_);
    runtime_ = nullptr;
}

bool ${interface}AbstractService::Start()
{
    if (service_ == nullptr) {
        return false;
    }

    service_->start(service_, ${interface}RequestHandler, this,
                    ${interface}SessionHandler, this,
                    ${interface}CommHandler, this);

    return true;
}

void ${interface}AbstractService::Stop()
{
    if (service_ == nullptr) {
        return;
    }

    service_->stop(service_);
}

void ${interface}AbstractService::onRequest(PolarisReadableMessage* request)
{
    std::string request_name = request->get_name(request);
${request_case}
}

void ${interface}AbstractService::${interface}RequestHandler(
    void* user_data, PolarisReadableMessage* message)
{
    ${interface}AbstractService* impl =
        reinterpret_cast<${interface}AbstractService*>(user_data);

    if (impl == nullptr) {
        return;
    }

    impl->onRequest(message);
}

void ${interface}AbstractService::${interface}SessionHandler(
    void* user_data, const PolarisSession* session, bool active)
{
    ${interface}AbstractService* service =
        reinterpret_cast<${interface}AbstractService*>(user_data);

    if (service == nullptr) {
        return;
    }

    bool status = active > 0 ? true : false;
    SessionContext ctx;
    ctx.channel = session->channel;
    ctx.token = session->token;
    ctx.client_identifier = session->client_identifier;
    service->handleSession(ctx, status);
}

void ${interface}AbstractService::${interface}CommHandler(
    void* user_data, bool available)
{
    ${interface}AbstractService* service =
        reinterpret_cast<${interface}AbstractService*>(user_data);

    if (service == nullptr) {
        return;
    }

    bool status = available > 0 ? true : false;
    service->handleCommStatus(status);
}

void ${interface}AbstractService::initNameIdMapping()
{
    std::vector<std::string> all_names = {${method_event_names}
                                         };

    for(size_t i = 0; i < all_names.size(); i++) {
        name_id_map_->InsertNameId(all_names[i], i);
        name_id_map_->InsertIdName(i, all_names[i]);
    }
}
${on_methods}
${notifys}
""")


        content = temp.substitute(interface = interface_info[ir_parser.kName],
                                    request_case = self.__getAbstractServiceReqCase(interface_info),
                                    namespace = self._full_name_space,
                                    method_event_names = self._getMethodEventNamesStr(interface_info,
                                                            "\n                                    ", ","),
                                    on_methods = self.__getAbstractServiceMethods(interface_info),
                                    notifys = self.__getAbstractServiceEvents(interface_info))
        utils.Utils.writeGenFile(self._path, self._file, content)

    def __getAbstractServiceReqCase(self, interface_info):
        is_first = True
        result = ""

        if not ir_parser.kMethodList in interface_info.keys():
            return ""

        for method in interface_info[ir_parser.kMethodList]:
            method_name = method[ir_parser.kMethodName]

            if is_first:
                is_first = False
                result += """
    if (request_name == "%s") {
        std::string permission = "";
        on%s(request, permission);
    }
""" %(method_name, method_name)
            else:
                result += """
    else if (request_name == "%s") {
        std::string permission = "";
        on%s(request, permission);
    }
""" %(method_name, method_name)

        return result

    def __getAbstractServiceMethods(self, interface_info):
        result = ""
        in_args_str = ""

        if not ir_parser.kMethodList in interface_info.keys():
            return ""

        for item in interface_info[ir_parser.kMethodList]:
            in_args_str = ""
            out_args_str = ""
            no_type_out_args_str = ""
            reader_str = ""

            if ir_parser.kMethodParameter in item.keys()\
                and not self._isArgsListEmpty(item[ir_parser.kMethodParameter]):
                args_list = item[ir_parser.kMethodParameter]
                in_args_str = "," + self._getNoTypeArgListStr("in", args_list, "", ",")

                for arg in args_list:
                    arg_type = self._typeConvert(arg[ir_parser.kType])
                    arg_name = arg[ir_parser.kName]
                    if arg_type == "void":
                        reader_str = ""
                        break
                    reader_str += """                
    %s %s;
    reader.Read(&%s);                
""" %(arg_type, arg_name, arg_name)

            if ir_parser.kMethodReturn in item.keys()\
                and not self._isArgsListEmpty(item[ir_parser.kMethodReturn]):
                args_list = item[ir_parser.kMethodReturn]
                out_args_str = self._getArgListStr("out", args_list, "\n            const ", "&", ",")
                no_type_out_args_str = self._getNoTypeArgListStr("out", args_list, "\n                ", ",")
                temp = Template("""
    auto handler = [this, cloned_request](${out_args_str}) {
        ${interface}_${method}_Resp argument = {${no_type_out_args_str}
                                                       };
        service_->reply(service_, cloned_request,
                        ${interface}Codec::${method}ReplyDecorator, &argument);
        PolarisDestroySyncReplyMessage(cloned_request);
    };
    handle${method}(ctx${in_args_str}, handler); 
""")
            else:
                temp = Template("""
    handle${method}(ctx${in_args_str}); 
""")
            handler_str = temp.substitute(interface = interface_info[ir_parser.kName],
                                            method = item[ir_parser.kMethodName],
                                            out_args_str = out_args_str,
                                            in_args_str = in_args_str,
                                            no_type_out_args_str = no_type_out_args_str)
                
            temp = Template("""
void ${interface}AbstractService::on${method}(PolarisReadableMessage* request, const std::string& permission)
{
    MessageReader reader(request);
${reader_str}

    uint32_t channel = 0;
    request->get_channel(request, &channel);
    string token = request->get_token(request);
    bool check_result = true;

    if(!permission.empty()) {
        check_result = service_->verify_permission(service_, channel,
                       token.c_str(), permission.c_str());
    }

    SessionContext ctx;
    ctx.channel = channel;
    ctx.token = token;
    ctx.has_permission = check_result;
    PolarisReadableMessage* cloned_request = request->clone(request);
${handler_str}
}
""")
            result += temp.substitute(interface = interface_info[ir_parser.kName],
                                  method = item[ir_parser.kMethodName],
                                  reader_str = reader_str,
                                  handler_str = handler_str)

        return result

    def __getAbstractServiceEvents(self, interface_info):
        result = ""
        if not ir_parser.kMethodList in interface_info.keys():
            return ""

        for item in interface_info[ir_parser.kEventList]:
            args = ""
            no_type_args = ""

            if ir_parser.kMembers in item \
                and not self._isArgsListEmpty(item[ir_parser.kMembers]):
                args_list = item[ir_parser.kMembers]
                args = self._getArgListStr("in", args_list, "\n        const ", "&", ",")
                no_type_args = self._getNoTypeArgListStr("in", args_list, "", ",")

                temp = Template("""
void ${interface}AbstractService::Notify${event}(${args})
{
    if (service_ == nullptr) {
        return;
    }

    ${interface}_${event}_Notify argument = {${no_type_args}};
    service_->notify(service_, "${event}",
                     ${interface}Codec::${event}NotifyDecorator, &argument);
}

""")
            else:
                temp = Template("""
void ${interface}AbstractService::Notify${event}()
{
    if (service_ == nullptr) {
        return;
    }

    service_->notify(service_, "${event}",
                     nullptr, nullptr);
}
""")  
            result += temp.substitute(interface = interface_info[ir_parser.kName],
                                        event = item[ir_parser.kEventName],
                                        args = args,
                                        no_type_args = no_type_args)
        return result

    def __getServiceClassRegHandlerStr(self, interface_info):
        result = ""
        if not ir_parser.kMethodList in interface_info.keys():
            return result

        for item in interface_info[ir_parser.kMethodList]:
            temp = Template("""
void ${interface}Service::Register${method}Handler(
    const ${method}Handler& handler)
{
    if (impl_ != nullptr) {
        impl_->Register${method}Handler(handler);
    }
}
""")
            result += temp.substitute(interface = interface_info[ir_parser.kName],
                                        method = item[ir_parser.kMethodName])


        return result

    def __getServiceClassNotifyStr(self, interface_info):
        result = ""
        if not ir_parser.kEventList in interface_info.keys():
            return result

        for item in interface_info[ir_parser.kEventList]:
            event_name = item[ir_parser.kEventName]
            args_list = item[ir_parser.kMembers]
            temp = Template("""
void ${interface}Service::Notify${event}(
${out_args})
{
    if (impl_ != nullptr) {
        impl_->Notify${event}(${no_type_out_args});
    }
}
""")
            out_args_str = self._getArgListStr("out", args_list, "    const ", "&", ",")
            no_type_out_args_str = self._getNoTypeArgListStr("out", args_list, "", ",")
            result += temp.substitute(interface = interface_info[ir_parser.kName], event = event_name,
                                    out_args = out_args_str, no_type_out_args = no_type_out_args_str)
        return result