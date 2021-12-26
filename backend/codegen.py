
import sys
import logging
sys.path.append("..")
import common.jsonIr_parser as ir_parser
import common.utils as utils
import cpp_with_ndk_gen.cpp_gen as cpp_ndk_gen


if __name__=="__main__":
    ret, ir_path, dst_path, base_name, gen_types = utils.Utils.parserCommand(sys.argv)

    if ret > 0:
        sys.exit(0)

    if ret < 0:
        logging.error("parserCommand failed")
        sys.exit(0)

    ir = ir_parser.JsonIRParser(ir_path)
    ret, ir_dict = ir.parse()

    if ret < 0:
        logging.error("JsonIRParser failed")
        sys.exit(0)

    for gen_type in gen_types:
        if gen_type == "ndk_cpp":
            cpp_ndk_gen.CppGenerator().gen(dst_path, base_name, ir_dict)



