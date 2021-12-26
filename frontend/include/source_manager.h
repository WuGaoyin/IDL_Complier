
#ifndef _ONE_IDLC_SOURCE_MANAGER_H_
#define _ONE_IDLC_SOURCE_MANAGER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "source_file.h"

namespace idlc {

class SourceManager {
 public:
  // Returns whether the filename was successfully read.
  bool CreateSource(std::string_view filename);
  void AddSourceFile(std::unique_ptr<SourceFile> file);

  const std::vector<std::unique_ptr<SourceFile>>& sources() const { return sources_; }

 private:
  std::vector<std::unique_ptr<SourceFile>> sources_;
};

}  // namespace idlc

#endif  // _ONE_IDLC_SOURCE_MANAGER_H_
