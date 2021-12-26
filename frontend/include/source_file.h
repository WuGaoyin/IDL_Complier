
#ifndef _ONE_IDLC_SOURCE_FILE_H_
#define _ONE_IDLC_SOURCE_FILE_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace idlc {

class SourceFile {
 public:
  SourceFile(std::string filename, std::string data);
  virtual ~SourceFile();

  // Enables move construction and assignment
  SourceFile(SourceFile&& rhs) = default;
  SourceFile& operator=(SourceFile&&) = default;

  // no copy or assign (move-only or pass by reference)
  SourceFile(const SourceFile&) = delete;
  SourceFile& operator=(const SourceFile&) = delete;

  std::string_view filename() const { return filename_; }
  std::string_view data() const { return data_; }

  // This is in the coordinates that most editors use. Lines start
  // at 1 and columns start at 1.
  struct Position {
    int line;
    int column;
  };

  // Returns the line and `Position` containing a specified span.
  //
  // Parameters:
  //  * view:          The span to search for.
  //  * position_out:  Where to output the `Position` of the span within the `SourceFile`.
  //
  // Returns:
  //  * A `std::string_view` of the encompassing line. This line will not contain a newline
  //    character.
  virtual std::string_view LineContaining(std::string_view view, Position* position_out) const;

 private:
  std::string filename_;
  std::string data_;
  std::vector<std::string_view> lines_;
};

}  // namespace idlc

#endif  // _ONE_IDLC_SOURCE_FILE_H_
