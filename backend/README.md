# what is soa_srv_gen.py?
******************
soa_srv_gen.py is a SOA service framework coding generator tool by apollo one idl.

## how to use soa_srv_gen.py?
******************
Python3 is recommended
Before you use this tool, install Python in your enviroment

codegen.py usage:
    python codegen.py [--help] [-p <dest_path>] [-t <type>] [-b <basename>]
                      [-i <json_ir_path>] [-l <log_level>]
    param description:
        --help    help information
        -p        path to generated file
        -t        generated file type, cpp, java ...
        -b        base name, it will effect the generated file name
        -i        file name to json ir file
        -l        log level of the tools

    example:
        python codegen.py -p ./gen -t cpp -i ./test/classinfo.json -b ClassInfo