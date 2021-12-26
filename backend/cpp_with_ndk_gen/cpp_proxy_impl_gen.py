import logging
from string import Template
import sys
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
from . import cpp_gen_protocol


class CppProxyImplGenerator(cpp_gen_protocol.CppGeneratorProtocol):
    def __init__(self, path, base_name, ir_dict):
        super(CppProxyImplGenerator, self).__init__(path, base_name, base_name + "Proxy.cpp", ir_dict)

    def gen(self):
        print("start to gen proxy impl")
        self.__genIncAndTypeDefs()
        self._genNameSpaceStart()
        self.__genStableLines()
        self.__genImpl()
        self._genNameSpaceEnd()


    def __genIncAndTypeDefs(self):
        content_lines = '''
#include "%sProxy.h"
#include <mutex>
''' % (self._base_name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genStableLines(self):
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

static ErrorCode convert(PolarisErrorCode error)
{
    if (error == kSuccess) {
        return ErrorCode::SUCCESS;
    }
    if (error == kNoConnection) {
        return ErrorCode::NO_SERVICE;
    }
    if (error == kRequestFailed) {
        return ErrorCode::REQUEST_FAILED;
    }
    if (error == kTimeout) {
        return ErrorCode::TIME_OUT;
    }
    return ErrorCode::INTERNAL_ERROR;
}

static void Service_status_handler(void* user_data, bool status)
{
    ServiceStatusCallback* callback =
        reinterpret_cast<ServiceStatusCallback*>(user_data);

    if (callback == nullptr) {
        return;
    }

    (*callback)(status);
}
"""
        utils.Utils.writeGenFile(self._path, self._file, content_lines) 

    def __genImpl(self):
        if not ir_parser.kDeclarationsOrder in self._ir_dict.keys():
            print("no key of " + ir_parser.kDeclarationsOrder)
            return

        for order_item in self._ir_dict[ir_parser.kDeclarationsOrder]:
            if order_item[ir_parser.kCategory] == ir_parser.kInterface:
                self.__genInterfaceImpl(order_item[ir_parser.kName])
    
    def __genInterfaceImpl(self, name):
        for item in self._ir_dict[ir_parser.kInterfaceDeclarations]:
            if not item[ir_parser.kName] == name:
                continue
            self.__genImplUserDataDecl(name)
            self.__genCodec(item)
            self.__genImplClass(item)
            self.__genProxyClass(item)

    def __genImplUserDataDecl(self, name):
        content_lines = """
class %sProxyImpl;

struct %sProxyImplUserData {
    %sProxyImpl* impl = nullptr;
    void* inner = nullptr;
};
""" %(name, name, name)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __genCodec(self, interface_info):
        interface_name = interface_info[ir_parser.kName]
        method_str = self.__getCodecMethodsStr(interface_info)
        content_lines = """
class %sCodec
{
public:
%s
};
""" %(interface_name, method_str)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __getCodecMethodsStr(self, interface_info):
        result = ""
        if not ir_parser.kMethodList in interface_info.keys():
            return result
        
        for method_item in interface_info[ir_parser.kMethodList]:
            method_name = method_item[ir_parser.kMethodName]   
            method_body = self.__getCodecMethodBodyStr(interface_info[ir_parser.kName], method_item)                
            result += """
    static void %s_message_decorator(
        void* user_data, PolarisWritableMessage* message)
    {
%s
    }
""" %(method_name, method_body)           
        return result

    def __getCodecMethodBodyStr(self, interface_name, method_info):
        args_str = ""
        if not ir_parser.kMethodParameter in method_info.keys() \
            or self._isArgsListEmpty(method_info[ir_parser.kMethodParameter]):
            return ""

        for arg in method_info[ir_parser.kMethodParameter]:
             args_str += "        writer.Write(argument->%s);\n" %(arg[ir_parser.kName])

        temp = Template("""
        ${interface}_${method}_Req* argument = reinterpret_cast<${interface}_${method}_Req*>(user_data);

        if (argument == nullptr) {
            return;
        }

        MessageWriter writer(message);
        message->serialize_begin(message, ${num});
${args_content}
        message->serialize_end(message);
""")
        return temp.substitute(interface = interface_name, 
                                method = method_info[ir_parser.kMethodName],
                                num = len(method_info[ir_parser.kMethodParameter]),
                                args_content = args_str)

    def __genImplClass(self, interface_info):
        interface_name = interface_info[ir_parser.kName]
        call_backs_str = ""
        if ir_parser.kMethodList in interface_info.keys():
            for method in interface_info[ir_parser.kMethodList]:
                name = method[ir_parser.kMethodName]

                if ir_parser.kMethodReturn in method.keys() \
                    and not self._isArgsListEmpty(method[ir_parser.kMethodReturn]):
                    call_backs_str += "    std::vector<std::shared_ptr<%sProxy::%sCallback>> %s_callbacks_;\n"\
                            %(interface_name, name, name)

        if ir_parser.kEventList in interface_info.keys():
            for event in interface_info[ir_parser.kEventList]:
                name = event[ir_parser.kEventName]
                call_backs_str += "    std::vector<std::shared_ptr<%sProxy::%sCallback>> %s_callbacks_;\n"\
                         %(interface_name, name, name)

        temp = Template("""
class ${interface}ProxyImpl final
{
public:
    ${interface}ProxyImpl(const std::string& app_name)
    {
        runtime_ = PolarisCreateRuntime();
        name_id_map_ = std::make_shared<NameIdMapper>();

        if (runtime_ == nullptr || name_id_map_ == nullptr) {
            return;
        }

        initNameIdMapping();

        PolarisServiceIdentifier identifier;
        identifier.service_name = "${namespace}.${interface}";
        client_ = PolarisCreateClient(runtime_, &identifier, POLARIS_CHANNEL_DDS,
                                     app_name.c_str(), NameToId, &name_id_map_);
    }

    ~${interface}ProxyImpl()
    {
        if (runtime_ == nullptr) {
            return;
        }

        if (client_ != nullptr) {
            PolarisDestroyClient(runtime_, client_);
            client_ = nullptr;
        }

        PolarisDestroyRuntime(runtime_);
        runtime_ = nullptr;
    }

    void WatchServiceStatus(const ServiceStatusCallback& callback)
    {
        if (client_ == nullptr) {
            return;
        }

        mutex_.lock();
        auto backup = std::make_shared<ServiceStatusCallback>(callback);
        ServiceStatus_callbacks_.push_back(backup);
        mutex_.unlock();

        client_->watch_service_status(
            client_, Service_status_handler, backup.get());
    }

    bool IsServiceActive()
    {
        if (client_ == nullptr) {
            return false;
        }

        return client_->is_service_active(client_);
    }

    WaitResult WaitService(int timeout)
    {
        if (client_ == nullptr) {
            return WaitResult::kFailed;
        }
        auto result = client_->wait_service(client_, timeout);
        switch (result) {
        case kWaitResultSuccess:
            return WaitResult::kSuccess;
        case kWaitResultTimeout:
            return WaitResult::kTimeout;
        case kWaitFailed:
            return WaitResult::kFailed;
        default:
            return WaitResult::kFailed;
        }
    }

    void Unwatch(const std::string& event_name)
    {
        if (client_ == nullptr) {
            return;
        }

        client_->unwatch(client_, event_name.c_str());
    }

${methods_str}
${events_str}

private:
    void initNameIdMapping()
    {
        std::vector<std::string> all_names = {${method_event_names}
                                             };

        for(size_t i = 0; i < all_names.size(); i++) {
            name_id_map_->InsertNameId(all_names[i], i);
        }
    }

private:
    std::shared_ptr<NameIdMapper> name_id_map_;
    PolarisRuntime* runtime_ = nullptr;
    PolarisClient* client_ = nullptr;
    std::recursive_mutex mutex_;
    std::vector<std::shared_ptr<${interface}ProxyImplUserData>> user_data_list_;
    std::vector<std::shared_ptr<ServiceStatusCallback>> ServiceStatus_callbacks_;
${call_backs}
};  
""")        
        content_lines = temp.substitute(namespace = self._full_name_space,
                                        interface = interface_info[ir_parser.kName],
                                        methods_str = self.__getImplClassMethodsStr(interface_info),
                                        events_str = self.__getImplClassEventsStr(interface_info),
                                        call_backs = call_backs_str,
                                        method_event_names = self._getMethodEventNamesStr(interface_info,
                                                            "\n                                             ", ","))

        utils.Utils.writeGenFile(self._path, self._file, content_lines)
        
    def __getImplClassMethodsStr(self, interface_info):
        result = ""
        interface_name = interface_info[ir_parser.kName]
        if not ir_parser.kMethodList in interface_info.keys():
            return ""

        for method in interface_info[ir_parser.kMethodList]:
            if ir_parser.kMethodReturn in method.keys()\
                and not self._isArgsListEmpty(method[ir_parser.kMethodReturn]):
                result += self.__getImplClassMethodSync(interface_name, method)
                result += self.__getImplClassMethodAsync(interface_name, method)
            else:
                result += self.__getImplClassMethod(interface_name, method)

        return result

    def __getImplClassEventsStr(self, interface_info):
        result = ""
        interface_name = interface_info[ir_parser.kName]
        if not ir_parser.kEventList in interface_info.keys():
            return ""
        for event in interface_info[ir_parser.kEventList]:  
            result += self.__getImplClassEvent(interface_name, event)

        return result

    def __getImplClassMethodSync(self, interface_name, method_info):
        no_type_in_args_str = ""
        in_args_str = ""
        req_arg_define_str = ""
        req_arg_str = "nullptr"
        method_name = method_info[ir_parser.kMethodName]
        out_args_str = self._getArgListStr("out", method_info[ir_parser.kMethodReturn],
                                            "", "*", ",")
        if ir_parser.kMethodParameter in method_info.keys():
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = self._getArgListStr("in", args_list, "const ", "&", ",")
            no_type_in_args_str = self._getNoTypeArgListStr("in", args_list, "", ",")

            if not self._isArgsListEmpty(args_list):
                req_arg_define_str = "%s_%s_Req inner_argument = {%s};" %(interface_name,
                                                    method_name, no_type_in_args_str)
                req_arg_str = "&inner_argument"
                in_args_str += ","

        reader_content_str = ""
        i = 0
        for out_arg in method_info[ir_parser.kMethodReturn]:
            if ir_parser.kName in out_arg.keys():
                reader_content_str += "        reader.Read(%s);\n" % (out_arg[ir_parser.kName])
            else:
                reader_content_str += "        reader.Read(out_arg_%d);\n" % (i)
            i += 1

        temp = Template("""
    ErrorCode ${method}Sync(
            ${in_args}
            ${out_args},
            int timeout_msec)
    {
        if (client_ == nullptr) {
            return ErrorCode::REQUEST_FAILED;
        }

        ${req_arg_define}
        auto inner_result = client_->request_sync(
                                client_, "${method}", timeout_msec,
                                 ${interface}Codec::${method}_message_decorator, ${req_arg});

        PolarisErrorCode inner_error = inner_result.error_code;

        if (inner_error != kSuccess) {
            return convert(inner_error);
        }

        auto inner_message = inner_result.reply;

        if (inner_message == nullptr) {
            return ErrorCode::PARAM_INVALID;
        }

        MessageReader reader(inner_message);
${reader_content}
        PolarisDestroySyncReplyMessage(inner_message);
        return ErrorCode::SUCCESS;
    }
""")
        return temp.substitute(interface = interface_name,
                                method = method_name,
                                in_args = in_args_str,
                                out_args = out_args_str,
                                req_arg_define = req_arg_define_str,
                                req_arg = req_arg_str,
                                reader_content = reader_content_str)

    def __getImplClassMethodAsync(self, interface_name, method_info):
        method_name = method_info[ir_parser.kMethodName]
        in_args_str = ""
        no_type_in_args_str = ""
        req_arg_define_str = ""
        req_arg_str = "nullptr"

        if ir_parser.kMethodParameter in method_info.keys():
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = self._getArgListStr("in", args_list, "const ", "&", ",")
            no_type_in_args_str = self._getNoTypeArgListStr("in", args_list, "", ",")
        
            if not self._isArgsListEmpty(args_list):
                req_arg_define_str = "%s_%s_Req inner_argument = {%s};" %(interface_name,
                                                    method_name, no_type_in_args_str)
                req_arg_str = "&inner_argument"
                in_args_str += ","

        reader_content_str = ""
        out_args_str = ""
        out_null_str = ""
        i = 0
        for out_arg_item in method_info[ir_parser.kMethodReturn]:
            out_null_str += ", nullptr"
            out_arg_type = self._typeConvert(out_arg_item[ir_parser.kType])

            if ir_parser.kName in out_arg_item.keys():
                out_arg_name = out_arg_item[ir_parser.kName]
            else:
                out_arg_name = "out_arg_%d" %(i)
            reader_content_str += """
        %s %s;
        reader.Read(&%s);
""" % (out_arg_type, out_arg_name, out_arg_name)               
            out_args_str += " ,&" + out_arg_name
            i += 1

        temp = Template("""
void ${method}Async(${in_args}
                           const ${interface}Proxy::${method}Callback& callback)
    {
        if (callback == nullptr) {
            return;
        }

        if (client_ == nullptr) {
            callback(ErrorCode::REQUEST_FAILED${out_null});
            return;
        }

        mutex_.lock();
        auto inner_backup = std::make_shared<${interface}Proxy::${method}Callback>(callback);
        ${method}_callbacks_.push_back(inner_backup);
        std::shared_ptr<${interface}ProxyImplUserData> inner_user_data =
            std::make_shared<${interface}ProxyImplUserData>();
        inner_user_data->impl = this;
        inner_user_data->inner = inner_backup.get();
        user_data_list_.push_back(inner_user_data);
        mutex_.unlock();

        ${req_arg_define}
        client_->request_async(
            client_, "${method}",
            ${interface}Codec::${method}_message_decorator, ${req_arg},
            ${method}_result_handler, inner_user_data.get());

    }

    static void ${method}_result_handler(
        void* user_data, const PolarisRequestResult* result)
    {
        ${interface}ProxyImplUserData* data =
            reinterpret_cast<${interface}ProxyImplUserData*>(user_data);

        if(data == nullptr) {
            return;
        }

        if(data->impl == nullptr) {
            return;
        }

        ${interface}Proxy::${method}Callback* callback =
            reinterpret_cast<${interface}Proxy::${method}Callback*>(data->inner);

        if((callback == nullptr) || (result == nullptr)) {
            return;
        }

        if (result->error_code != kSuccess) {
            (*callback)(convert(result->error_code)${out_null});
            data->impl->Remove${method}ResultCallback(data->inner);
            return;
        }

        PolarisReadableMessage* message = result->reply;

        MessageReader reader(message);
${reader_content}

        (*callback)(convert(result->error_code)${out_args});
        data->impl->Remove${method}ResultCallback(data->inner);
    }

    void Remove${method}ResultCallback(void* user_data)
    {
        std::lock_guard<std::recursive_mutex> lk(mutex_);
        auto data_iter = user_data_list_.begin();

        for (; data_iter != user_data_list_.end(); data_iter++) {
            if ((*data_iter) == nullptr) {
                continue;
            }

            if ((*data_iter)->inner != user_data) {
                continue;
            }

            user_data_list_.erase(data_iter);
            break;
        }

        auto iter = ${method}_callbacks_.begin();

        for (; iter != ${method}_callbacks_.end(); iter++) {
            if ((*iter) == nullptr) {
                continue;
            }

            if ((*iter).get() != user_data) {
                continue;
            }

            ${method}_callbacks_.erase(iter);
            break;
        }
    }

""")
        return temp.substitute(interface = interface_name,
                                method = method_name,
                                in_args = in_args_str,
                                out_args = out_args_str,
                                req_arg_define = req_arg_define_str,
                                req_arg = req_arg_str,
                                reader_content = reader_content_str,
                                out_null = out_null_str)

    def __getImplClassMethod(self, interface_name, method_info):
        no_type_in_args_str = ""
        in_args_str = ""
        req_arg_define_str = ""
        req_arg_str = "nullptr"
        method_name = method_info[ir_parser.kMethodName]

        if ir_parser.kMethodParameter in method_info.keys():
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = self._getArgListStr("in", args_list, "const ", "&", ",")
            no_type_in_args_str = self._getNoTypeArgListStr("in", args_list, "", ",")
        
            if not self._isArgsListEmpty(args_list):
                req_arg_define_str = "%s_%s_Req inner_argument = {%s};" %(interface_name,
                                                    method_name, no_type_in_args_str)
                req_arg_str = "&inner_argument"
                
        temp = Template("""
    ErrorCode ${method}(${in_args}) const
    {
        if (client_ == nullptr) {
            return ErrorCode::REQUEST_FAILED;
        }

        ${req_arg_define}
        auto inner_result = client_->send(
                                client_, "${method}",
                                ${interface}Codec::${method}_message_decorator, ${req_arg});

        PolarisErrorCode inner_error = inner_result;

        if (inner_error != kSuccess) {
            return convert(inner_error);
        }

        return ErrorCode::SUCCESS;
    }
""")
        return temp.substitute(interface = interface_name,
                                method = method_name,
                                in_args = in_args_str,
                                req_arg_define = req_arg_define_str,
                                req_arg = req_arg_str)

    def __getImplClassEvent(self, interface_name, event_info):
        event_name = event_info[ir_parser.kEventName]
        reader_content_str = ""
        reader_args_str = ""

        if ir_parser.kMembers in event_info.keys() \
            and not self._isArgsListEmpty(event_info[ir_parser.kMembers]):
            reader_args_str = self._getNoTypeArgListStr("out", event_info[ir_parser.kMembers], "", ",")
            reader_content_str += "        MessageReader reader(message);\n"

            i = 0
            for arg in event_info[ir_parser.kMembers]:
                arg_type = self._typeConvert(arg[ir_parser.kType])

                if ir_parser.kName in arg.keys():
                    arg_name = arg[ir_parser.kName]
                else:
                    arg_name = "out_arg_%s" %(i)
                reader_content_str += """
        %s %s;
        reader.Read(&%s);
""" %(arg_type, arg_name, arg_name)
                i += 1

        temp = Template("""
    void On${event}(const ${interface}Proxy::${event}Callback& callback)
    {
        if (client_ == nullptr) {
            return;
        }

        if (callback == nullptr) {
            return;
        }

        mutex_.lock();
        auto backup = std::make_shared<${interface}Proxy::${event}Callback>(callback);
        ${event}_callbacks_.push_back(backup);
        mutex_.unlock();

        client_->watch(client_, "${event}",
                       ${event}_message_handler, backup.get());
    }

    static void ${event}_message_handler(
        void* user_data, PolarisReadableMessage* message)
    {
        ${interface}Proxy::${event}Callback* callback =
            reinterpret_cast<${interface}Proxy::${event}Callback*>(user_data);

        if (callback == nullptr) {
            return;
        }

${reader_content}

        (*callback)(${reader_args});
    }

    void Off${event}()
    {
        if (client_ == nullptr) {
            return;
        }

        client_->unwatch(client_, "${event}");
    }
""")
        return temp.substitute(interface = interface_name,
                                event = event_name,
                                reader_content = reader_content_str,
                                reader_args = reader_args_str)

    def __genProxyClass(self, interface_info):
        methods_content_str = self.__getProxyClassMethodsStr(interface_info)
        events_content_str = self.__getProxyClassEventsStr(interface_info)
        temp = Template("""
${interface}Proxy::${interface}Proxy(const std::string& app_name)
    : impl_(std::make_shared<${interface}ProxyImpl>(app_name)) {}

void
${interface}Proxy::WatchServiceStatus(const ServiceStatusCallback& callback)
{
    if (impl_ == nullptr) {
        return;
    }

    impl_->WatchServiceStatus(callback);
}

bool ${interface}Proxy::IsServiceActive()
{
    if (impl_ == nullptr) {
        return false;
    }

    return impl_->IsServiceActive();
}

WaitResult ${interface}Proxy::WaitService(int32_t timeout)
{
    if (impl_ == nullptr) {
        return WaitResult::kFailed;
    }

    return impl_->WaitService(timeout);
}

void ${interface}Proxy::Unwatch(const std::string& event_name)
{
    if (impl_ == nullptr) {
        return;
    }

    impl_->Unwatch(event_name);
}

${methods_content}
${events_content}
        """)
        content_lines = temp.substitute(interface = interface_info[ir_parser.kName],
                                    methods_content = methods_content_str,
                                    events_content = events_content_str)
        utils.Utils.writeGenFile(self._path, self._file, content_lines)

    def __getProxyClassMethodsStr(self, interface_info):
        result = ""
        interface_name = interface_info[ir_parser.kName]

        if not ir_parser.kMethodList in interface_info.keys():
            return ""
        for method in interface_info[ir_parser.kMethodList]:
            if ir_parser.kMethodReturn in method.keys() \
                and not self._isArgsListEmpty(method[ir_parser.kMethodReturn]):
                result += self.__getProxyClassMethodSync(interface_name, method)
                result += self.__getProxyClassMethodAsync(interface_name, method)
            else:
                result += self.__getProxyClassMethod(interface_name, method)

        return result

    def __getProxyClassEventsStr(self, interface_info):
        result = ""
        if not ir_parser.kEventList in interface_info.keys():
            return ""
        for event in interface_info[ir_parser.kEventList]:
            temp = Template("""
void ${interface}Proxy::On${event}(
    const ${event}Callback& callback)
{
    if (impl_ == nullptr) {
        return;
    }

    impl_->On${event}(callback);
}

void ${interface}Proxy::Off${event}()
{
    if (impl_ == nullptr) {
        return;
    }

    impl_->Off${event}();
}
""")
            result += temp.substitute(interface = interface_info[ir_parser.kName],
                                    event = event[ir_parser.kEventName])
        return result

    def __getProxyClassMethodSync(self, interface_name, method_info):
        no_type_in_args_str = ""
        in_args_str = ""
        args_list = method_info[ir_parser.kMethodReturn]        
        out_args_str = self._getArgListStr("out", args_list, "\n        ", "*", ",")
        no_type_out_args_str = self._getNoTypeArgListStr("out", args_list, "", ",")    

        if ir_parser.kMethodParameter in method_info.keys():
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = self._getArgListStr("in", args_list, "\n        const ", "&", ",")
            no_type_in_args_str = self._getNoTypeArgListStr("in", args_list, "", ",")
            if not in_args_str == "":
                in_args_str += ","
            if not no_type_in_args_str == "":
                no_type_in_args_str += ","

        temp = Template("""
ErrorCode ${interface}Proxy::${method}Sync(${in_args}${out_args},
        int timeout_msec)
{
    if (impl_ == nullptr) {
        return ErrorCode::INTERNAL_ERROR;
    }

    return impl_->${method}Sync(${no_type_in_args}${no_type_out_args},timeout_msec);
}
""")
        return temp.substitute(interface = interface_name,
                                method = method_info[ir_parser.kMethodName],
                                in_args = in_args_str,
                                out_args = out_args_str,
                                no_type_in_args = no_type_in_args_str,
                                no_type_out_args = no_type_out_args_str)

    def __getProxyClassMethodAsync(self, interface_name, method_info):
        no_type_in_args_str = ""
        in_args_str = ""
        
        if ir_parser.kMethodParameter in method_info.keys():
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = self._getArgListStr("in", args_list, "\n        const ", "&", ",")
            no_type_in_args_str = self._getNoTypeArgListStr("in", args_list, "", ",")
            if not in_args_str == "":
                in_args_str += ","
            if not no_type_in_args_str == "":
                no_type_in_args_str += ","

        temp = Template("""
void ${interface}Proxy::${method}Async(${in_args}
        const ${interface}Proxy::${method}Callback& callback)
{
    if (impl_ == nullptr) {
        return;
    }

    impl_->${method}Async(${no_type_in_args}callback);
}
""")
        return temp.substitute(interface = interface_name,
                                method = method_info[ir_parser.kMethodName],
                                in_args = in_args_str,
                                no_type_in_args = no_type_in_args_str)

    def __getProxyClassMethod(self, interface_name, method_info):
        no_type_in_args_str = ""
        in_args_str = ""
        
        if ir_parser.kMethodParameter in method_info.keys():
            args_list = method_info[ir_parser.kMethodParameter]
            in_args_str = self._getArgListStr("in", args_list, "const ", "&", ",")
            no_type_in_args_str = self._getNoTypeArgListStr("in", args_list, "", ",")

        temp = Template("""
ErrorCode ${interface}Proxy::${method}(${in_args})
{
    if (impl_ == nullptr) {
        return ErrorCode::INTERNAL_ERROR;
    }

    return impl_->${method}(${no_type_in_args});
}
""")
        return temp.substitute(interface = interface_name,
                                method = method_info[ir_parser.kMethodName],
                                in_args = in_args_str,
                                no_type_in_args = no_type_in_args_str)
