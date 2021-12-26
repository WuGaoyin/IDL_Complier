
#include <iostream>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <sstream>
#include <fstream>
#include <sys/stat.h>

#include "log.h"
#include "source_manager.h"
#include "lexer.h"
#include "token.h"
#include "parser.h"
#include "compiled_ast.h"
#include "json_generator.h"

class ArgvArguments {
 public:
  ArgvArguments(int count, char** arguments)
      : count_(count), arguments_(const_cast<const char**>(arguments)) {}

  std::string Claim() {
    if (count_ < 1) {
      ALOGE("Missing part of an argument");
      exit(1);
    }
    std::string argument = arguments_[0];
    --count_;
    ++arguments_;

    return argument;
  }

  bool Remaining() const {
    return count_ > 0;
  }

 private:
  int count_;
  const char** arguments_;
};

void Usage() {
  ALOGI("usage: idlc -o [OUTPUT_PATH] -f [[FIDL_FILE...]...]");
  return;
}

void MakeParentDirectory(const std::string& filename) {
  std::string::size_type slash = 0;

  for (;;) {
    slash = filename.find('/', slash);
    if (slash == filename.npos) {
      return;
    }

    std::string path = filename.substr(0, slash);
    ++slash;
    if (path.size() == 0u) {
      // Skip creating "/".
      continue;
    }

    if (mkdir(path.data(), 0755) != 0 && errno != EEXIST) {
      ALOGE("Could not create directory %s for output file %s: error %s", path.data(),
           filename.data(), strerror(errno));
      exit(1);
    }
  }
}

std::fstream Open(std::string filename, std::ios::openmode mode) {
  if ((mode & std::ios::out) != 0) {
    MakeParentDirectory(filename);
  }

  std::fstream stream;
  stream.open(filename, mode);
  if (!stream.is_open()) {
    ALOGE("Could not open file: %s", filename.data());
    exit(1);
  }
  return stream;
}

void Write(const std::ostringstream& output_stream, const std::string file_path) {
  std::string contents = output_stream.str();
  struct stat st;
  if (!stat(file_path.c_str(), &st)) {
    // File exists.
    size_t current_size = st.st_size;
    if (current_size == contents.size()) {
      // Lengths match.
      std::string current_contents(current_size, '\0');
      std::fstream current_file = Open(file_path, std::ios::in);
      current_file.read(current_contents.data(), current_size);
      if (current_contents == contents) {
        // Contents match, no need to write the file.
        return;
      }
    }
  }
  std::fstream file = Open(file_path, std::ios::out);
  file << output_stream.str();
  file.flush();
  if (file.fail()) {
    ALOGE("Failed to flush output to file: %s", file_path.c_str());
    exit(1);
  }
}

std::unique_ptr<idlc::raw::File> Parse(const idlc::SourceFile& source_file) {
  idlc::Lexer lexer(source_file);
  idlc::Parser parser(&lexer);
  auto ast = parser.Parse();
  if (!parser.Success()) {
    ALOGE("parse file not success");
    return nullptr;
  }
  return std::move(ast);
}

int compile(const std::vector<std::string>& source_list,
            const std::vector<idlc::SourceManager>& source_managers,
            const std::string output_path) {
  std::unique_ptr<idlc::raw::File> final_ast = nullptr;
  for (const auto& source_manager : source_managers) {
    for (const auto& source_file : source_manager.sources()) {
      final_ast = Parse(*source_file);
      if (nullptr == final_ast) {
        return 1;
      }
    }
  }

  idlc::CompiledAST compiled_ast(std::move(final_ast));
  if (!compiled_ast.Compile()) {
    ALOGE("ast compile failed.");
    return 1;
  };

  idlc::JSONGenerator generator(&compiled_ast);
  Write(generator.Produce(), output_path);

  return 0;
}

int main(int argc, char* argv[]) {
    auto args = std::make_unique<ArgvArguments>(argc, argv);

    // Parse the program name.
    args->Claim();
    if (!args->Remaining()) {
        Usage();
        exit(0);
    }
    ALOGD("idlc_frontend process start");
    log_module_init();

    std::string output_path;
    while (args->Remaining()) {
      std::string behavior_argument = args->Claim();
      if ("-h" == behavior_argument) {
        Usage();
        exit(0);
      } else if ("-o" == behavior_argument) {
        output_path = args->Claim();
      } else if ("-f" == behavior_argument) {
        break;
      }
    }

    if (output_path.empty()) {
      Usage();
      exit(0);
    }

    // Prepare source files.
    std::vector<idlc::SourceManager> source_managers;
    std::vector<std::string> source_list;
    source_managers.push_back(idlc::SourceManager());
    while (args->Remaining()) {
        std::string arg = args->Claim();
        if (arg == "-f") {
            source_managers.emplace_back();
        } else {
            if (!source_managers.back().CreateSource(arg.data())) {
                ALOGE("Couldn't read in source data from %s", arg.data());
                exit(1);
            }
            source_list.emplace_back(arg.data());
        }
    }

    auto status = compile(source_list, source_managers, output_path);
}