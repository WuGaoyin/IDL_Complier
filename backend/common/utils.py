import os
import sys
import logging
import platform

class ColorsPrint:
    HEADER='\033[95m'
    OKBLUE='\033[94m'
    OKGREEN='\033[92m'
    WARNING='\033[93m'
    FALL='\033[91m'
    ENDC='\033[0m'
    BOLD='\033[1m'
    UNDERLINE='\033[4m'
    
    @staticmethod
    def PrintI(str):
        print(ColorsPrint.BOLD + str + ColorsPrint.ENDC)

    @staticmethod
    def PrintS(str):
        print(ColorsPrint.OKGREEN + str + ColorsPrint.ENDC)

    @staticmethod
    def PrintE(str):
        print(ColorsPrint.FALL + str + ColorsPrint.ENDC)

    @staticmethod
    def PrintH(str):
        print(ColorsPrint.OKBLUE + str + ColorsPrint.bcolors.ENDC)

class Utils:
    @staticmethod
    def helpinfo():
        print("""
codegen.py usage:
    python codegen.py [--help] [-p <dest_path>] [-t <type>] [-b <basename>]
                      [-i <json_ir_path>] [-l <log_level>]
    param description:
        --help    help information
        -p        path to generated file
        -t        generated file type, cpp, java, ndk_cpp...
        -b        base name, it will effect the generated file name
        -i        file name to json ir file
        -l        log level of the tools

    example:
        python codegen.py -p ./gen -t cpp -i ./test/classinfo.json -b ClassInfo    

""")

    @staticmethod
    def setLogLevel(inpara):
        tmp = logging.ERROR
        if inpara == "d":
            tmp = logging.DEBUG
        elif inpara == "i":
            tmp = logging.INFO
        elif inpara == "w":
            tmp = logging.WARNING
        elif inpara == "c":
            tmp = logging.CRITICAL
        else:
            tmp = logging.ERROR

        logging.basicConfig(format='%(asctime)s-%(filename)s[line:%(lineno)d]-%(levelname)s:%(message)s',
                        level=tmp)

    @staticmethod
    def getDirectoryPath(file_path):
        split_char = ""
        if platform.system().lower() == 'windows':
            split_char = "\\"
        else:
            split_char = "/"
        dir_path = file_path.split(split_char)[-1]
        dir_path = file_path[0:-1 * (len(dir_path))]
        return dir_path
 
    @staticmethod
    def parserCommand(args):
        dst_path = "."
        ir_path = "."
        log_level = "e"
        base_name = ""
        gen_types = []
        size = len(args)
        for i in range(size):
            if args[i] == "-h" or args[i] == "--h" or args[i] == "--help":
                Utils.helpinfo()
                return 1, ir_path, dst_path, base_name, gen_types
            if args[i] == "-p":
                if (i+1) >= size:
                    ColorsPrint.PrintE("error:lack of path")
                    Utils.helpinfo()
                    return -1, ir_path, dst_path, base_name, gen_types

                dst_path = args[i+1]
            if args[i] == "-t":
                if (i+1) >= size:
                    Utils.helpinfo()
                    return -1, ir_path, dst_path, base_name, gen_types
                if (args[i+1] == "cpp") or (args[i+1] == "java") or (args[i+1] == "ndk_cpp"):
                    gen_types.append(args[i+1])
            if args[i] == "-b":
                if (i+1) >= size:
                    Utils.helpinfo()
                    return -1, ir_path, dst_path, base_name, gen_types
                base_name = args[i+1]

            if args[i] == "-i":
                if (i+1) >= size:
                    Utils.helpinfo()
                    return -1, ir_path, dst_path, base_name, gen_types
                ir_path = args[i+1]
            if args[i] == "-l":
                if (i+1) >= size:
                    Utils.helpinfo()
                    return -1, ir_path, dst_path, base_name, gen_types
                log_level = args[i+1]

        Utils.setLogLevel(log_level)
        if len(gen_types) == 0 or len(base_name) == 0:
            return -1, ir_path, dst_path, base_name, gen_types
        return 0, ir_path, dst_path, base_name, gen_types

    @staticmethod
    def createGenFile(path, filename):
        if os.path.exists(path) == False:
            os.makedirs(path)

        full_name = path
        if platform.system().lower() == 'windows':
            full_name += "\\" + filename
        else:
            full_name += "/" + filename

        if os.path.exists(full_name):
            os.remove(full_name)
        os.mknod(full_name)

    @staticmethod
    def writeGenFile(path, filename, content_lines):
        if os.path.exists(path) == False:
            os.makedirs(path)

        full_name = path
        if platform.system().lower() == 'windows':
            full_name += "\\" + filename
        else:
            full_name += "/" + filename
               
        with open(full_name, 'a+') as f:
            for item in content_lines:
                f.writelines(item)
