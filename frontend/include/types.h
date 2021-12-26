
#ifndef _ONE_IDLC_TYPES_H_
#define _ONE_IDLC_TYPES_H_

#include <stdint.h>

namespace idlc {
namespace types {

using RightsWrappedType = uint32_t;

enum struct Nullability {
  kNullable,
  kNonnullable,
};

enum struct Strictness {
  kFlexible,
  kStrict,
};

enum struct Resourceness {
  kValue,
  kResource,
};

// TODO: zircon/types.h's zx_obj_type_t and related values must be
// kept in sync with this. Eventually, they will be generated from
// idl declarations. This is currently tested by idl-compiler's
// TypesTests's handle_subtype test.
// TODO: Remove this enumeration once handle generalization is
// fully implemented. The enum `obj_type` defined in the IDL library zx will
// become the only source of truth.
enum struct HandleSubtype : uint32_t {
  // special case to indicate subtype is not specified.
  kHandle = 0,

  kBti = 24,
  kChannel = 4,
  kClock = 30,
  kEvent = 5,
  kEventpair = 16,
  kException = 29,
  kFifo = 19,
  kGuest = 20,
  kInterrupt = 9,
  kIommu = 23,
  kJob = 17,
  kLog = 12,
  kMsiAllocation = 32,
  kPager = 28,
  kPciDevice = 11,
  kPmt = 26,
  kPort = 6,
  kProcess = 1,
  kProfile = 25,
  kResource = 15,
  kSocket = 14,
  kStream = 31,
  kSuspendToken = 27,
  kThread = 2,
  kTimer = 22,
  kVcpu = 21,
  kVmar = 18,
  kVmo = 3,
};

enum struct PrimitiveSubtype {
  kBool,
  kInt8,
  kInt16,
  kInt32,
  kInt64,
  kUint8,
  kUint16,
  kUint32,
  kUint64,
  kFloat32,
  kFloat64,
};

enum struct MessageKind {
  kRequest,
  kResponse,
  kEvent,
};

}  // namespace types
}  // namespace idlc

#endif  // _ONE_IDLC_TYPES_H_
