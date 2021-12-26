import logging
import sys
sys.path.append("..")
import common.jsonIr_parser
from . import cpp_common_header_gen
from . import cpp_common_impl_gen
from . import cpp_service_header_gen
from . import cpp_service_impl_gen
from . import cpp_proxy_header_gen
from . import cpp_proxy_impl_gen

class CppGenerator():
    def gen(self, path, base_name, ir_dict):
        common_header_gen = cpp_common_header_gen.CppCommonHeaderGenerator(path, base_name, ir_dict)
        common_header_gen.gen()

        common_impl_gen = cpp_common_impl_gen.CppCommonImplGenerator(path, base_name, ir_dict)
        common_impl_gen.gen()

        service_header_gen = cpp_service_header_gen.CppServiceHeaderGenerator(path, base_name, ir_dict)
        service_header_gen.gen()
   
        service_impl_gen = cpp_service_impl_gen.CppServiceImplGenerator(path, base_name, ir_dict)
        service_impl_gen.gen()

        proxy_header_gen = cpp_proxy_header_gen.CppProxyHeaderGenerator(path, base_name, ir_dict)
        proxy_header_gen.gen()

        proxy_impl_gen = cpp_proxy_impl_gen.CppProxyImplGenerator(path, base_name, ir_dict)
        proxy_impl_gen.gen()
