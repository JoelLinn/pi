#ifndef PI_H
#define PI_H

#include <cassert>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace pi {

// ----------------------------------------------------------------------------
// Utils
// ----------------------------------------------------------------------------

// Platform specific calling conventions
#if defined(__x86_64__) || defined(_WIN64)
// there is only one calling convention for x64
#define PI_CALLCON_STDCALL
#define PI_CALLCON_CDECL
#elif _MSC_VER
#define PI_CALLCON_STDCALL __stdcall
#define PI_CALLCON_CDECL __cdecl
#else
#define PI_CALLCON_STDCALL __attribute__((stdcall))
#define PI_CALLCON_CDECL __attribute__((cdecl))
#endif

// Return codes for COM safe calls - related to HRESULT
enum class SafeResult : uint32_t {
  OK = 0x00000000,
  ENOTIMPL = 0x80004001,
  ENOINTERFACE = 0x80004002,
  EPOINTER = 0x80004003,
  EFAIL = 0x80004005
};

#ifdef _WIN32
#define PI_SAFECALL_CALLTYPE PI_CALLCON_STDCALL
#else
#define PI_SAFECALL_CALLTYPE PI_CALLCON_CDECL
#endif
#define PI_SAFECALL_API pi::SafeResult PI_SAFECALL_CALLTYPE

// Wrapper to block exceptions from escaping c++ code
pi::SafeResult wrap_exceptions(std::function<pi::SafeResult()> f) {
  try {
    return f();
  } catch (...) {
    std::cerr << "PI: uncaught C++ exception: ";
    try {
      throw;
    } catch (const std::exception& e) {
      std::cerr << e.what();
    } catch (...) {
      std::cerr << "UNKNWON";
    }
    std::cerr << std::endl;
    return pi::SafeResult::EFAIL;
  }
}

// Check args for nullptr and return. Compiler will clean this.
#define PI_SAFECALL_CHECKARGS(...)                                             \
  {                                                                            \
    const void* ps[] = {__VA_ARGS__};                                          \
    for (int i = 0; i < sizeof(ps) / sizeof(void*); i++)                       \
      if (!ps[i])                                                              \
        return pi::SafeResult::EPOINTER;                                       \
  }

// Binary GUID support and text interpreter
typedef struct _GUID {
  unsigned int Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char Data4[8];
} GUID;

inline bool operator==(_GUID const& a, _GUID const& b) {
  return !memcmp(&a, &b, sizeof(_GUID));
}

// ascii table until 'f'
unsigned char constexpr hex2bin[] = {
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, /* 0x00 */
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, /* 0x10 */
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, /* 0x20 */
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  99, 99, 99, 99, 99, 99, /* 0x30 */
    99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99, /* 0x40 */
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, /* 0x50 */
    99, 10, 11, 12, 13, 14, 15                                      /* 0x60 */
};

constexpr std::optional<GUID> string_to_guid(char const* s) {
  /* XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX */

  GUID id = {};
  for (int i = 0; i < 37; ++i) {
    // we implicitly check string length here.
    // everything except the end should be non zero - the desired end should be
    // exactly zero
    switch (i) {
      // check dashes
    case 8:
    case 13:
    case 18:
    case 23:
      if (s[i] != '-')
        return std::nullopt;
      break;
      // check end of string
    case 36:
      if (s[i] != 0)
        return std::nullopt; // string is to long
      break;
      // check hex characters (including premature string termination)
    default:
      // table is only that long
      if (s[i] > 'f')
        return std::nullopt;
      // if the value is greater, the char was illegal
      if (hex2bin[s[i]] > 15)
        return std::nullopt;
      break;
    }
  }

  id.Data1 = (hex2bin[s[0]] << 28 | hex2bin[s[1]] << 24 | hex2bin[s[2]] << 20 |
              hex2bin[s[3]] << 16 | hex2bin[s[4]] << 12 | hex2bin[s[5]] << 8 |
              hex2bin[s[6]] << 4 | hex2bin[s[7]]);
  id.Data2 = hex2bin[s[9]] << 12 | hex2bin[s[10]] << 8 | hex2bin[s[11]] << 4 |
             hex2bin[s[12]];
  id.Data3 = hex2bin[s[14]] << 12 | hex2bin[s[15]] << 8 | hex2bin[s[16]] << 4 |
             hex2bin[s[17]];

  id.Data4[0] = hex2bin[s[19]] << 4 | hex2bin[s[20]];
  id.Data4[1] = hex2bin[s[21]] << 4 | hex2bin[s[22]];
  id.Data4[2] = hex2bin[s[24]] << 4 | hex2bin[s[25]];
  id.Data4[3] = hex2bin[s[26]] << 4 | hex2bin[s[27]];
  id.Data4[4] = hex2bin[s[28]] << 4 | hex2bin[s[29]];
  id.Data4[5] = hex2bin[s[30]] << 4 | hex2bin[s[31]];
  id.Data4[6] = hex2bin[s[32]] << 4 | hex2bin[s[33]];
  id.Data4[7] = hex2bin[s[34]] << 4 | hex2bin[s[35]];
  return id;
}

// Non terminated fixed allocated string type.
struct ShortString {
  uint8_t len;
  char str[std::numeric_limits<typeof(len)>::max()];

  static const size_t MAX_LENGTH = sizeof(str);
};
static_assert(sizeof(ShortString) == 256);

// ----------------------------------------------------------------------------
// IUnknown interface description
// ----------------------------------------------------------------------------

#define PI_IUNKNOWN_CALLTYPE PI_SAFECALL_CALLTYPE
struct IUnknown_IID {
  static constexpr char guid[] = "00000000-0000-0000-C000-000000000046";
};

// This would be the real IUnknown but proxying it solves ambiguity problems...
struct IUnknownBase {
public:
  virtual SafeResult PI_IUNKNOWN_CALLTYPE QueryInterface(GUID const* pguid,
                                                         void** ppvObject) = 0;

  virtual int PI_IUNKNOWN_CALLTYPE AddRef() = 0;

  virtual int PI_IUNKNOWN_CALLTYPE Release() = 0;
};

template <typename IID> struct PascalInterface : public IUnknownBase {
protected:
  // Define a static constexpr function to retrieve the guid of the interface.
  // In VC++ compatible compilers there exists __declspec(uuid("..."))
  static constexpr pi::GUID __guid() {
    constexpr auto id = string_to_guid(IID::guid);
    static_assert(id, "GUID is invalid");
    return id.value();
  };
};

// Proxy to the 'real' interface. It's convenient to add the guid here as well.
struct IUnknown : PascalInterface<IUnknown_IID> {};

// ----------------------------------------------------------------------------
// Generic IUnknown interface implementation
// ----------------------------------------------------------------------------

template <typename... Interfaces>
class UnknownImpl : public IUnknown, public Interfaces... {
public:
  UnknownImpl()
      : refCountCom(0), refCountSharedPtr(0), refStarted(false), refMutex() {}
  virtual ~UnknownImpl() {}

public:
  SafeResult PI_IUNKNOWN_CALLTYPE QueryInterface(const GUID* pguid,
                                                 void** ppvObject) override {
    if (!pguid || !ppvObject)
      return SafeResult::EPOINTER;

    // check for all implemented interfaces with recursive function
    if (QueryInterfaceImpl<IUnknown, Interfaces...>(*pguid, *ppvObject))
      return SafeResult::OK;
    else
      return SafeResult::ENOINTERFACE;
  }

private:
  // This template function expands recursively and checks all implemented
  // interfaces
  template <typename I1, typename... IRest>
  inline bool QueryInterfaceImpl(const GUID& guid, void*& pvObject) {
    constexpr GUID g = I1::__guid();
    if (guid == g) {
      // by definition we know I1 is implemented so we can do a fast static_cast
      pvObject = static_cast<I1*>(this);
      AddRef();
      return true;
    }

    if constexpr (sizeof...(IRest)) {
      // search next
      return QueryInterfaceImpl<IRest...>(guid, pvObject);
    } else {
      return false;
    }
  }

private:
  // ref count by COM-like interface consumers
  int refCountCom;

  // number of shared_ptr objects created and handed out
  int refCountSharedPtr;

  // this indicates whether the first reference has been added (calling AddRef()
  // or shared_ptr())
  bool refStarted;

  // mutex to lock reference counting in multi threaded environment
  std::mutex refMutex;

public:
  // Since we can not allow the user creating a smart pointer on an object that
  // does reference counting itself, we create it ourselves and hand it out if
  // requested. This way we ensure the object is only deleted if both reference
  // counting mechanisms count 0 references.
  template <typename T> std::shared_ptr<T> shared_ptr() {
    std::scoped_lock guard(refMutex);

    assertAddReference(guard);

    refStarted = true;
    refCountSharedPtr++;
    return std::shared_ptr<T>(dynamic_cast<T*>(this), [](T* obj) {
      // we can static_cast here since the dynamic_cast during creation must
      // have succeeded
      auto* uImpl = static_cast<UnknownImpl*>(obj);
      auto guard =
          std::make_unique<std::scoped_lock<std::mutex>>(uImpl->refMutex);

      assert(uImpl->refStarted);
      assert(uImpl->refCountSharedPtr > 0);
      if (!--(uImpl->refCountSharedPtr))
        uImpl->TryDelete(guard);
    });
  }

  int PI_IUNKNOWN_CALLTYPE AddRef() override {
    std::scoped_lock guard(refMutex);

    assertAddReference(guard);
    refStarted = true;
    return ++refCountCom;
  }

  int PI_IUNKNOWN_CALLTYPE Release() override {
    auto guard = std::make_unique<std::scoped_lock<std::mutex>>(refMutex);

    assert(refStarted);
    assert(refCountCom > 0);
    if (!--refCountCom) {
      // if there are no references in c++ code by shared_ptr this will be equal
      // to 'delete this'
      TryDelete(guard);
      // The moment we delete the instance we can NOT reference the member
      // variable 'refCount'!!!! Thus we hard code the return value.
      return 0;
    }
    return refCountCom;
  }

private:
  // Called when one reference counting mechanism sees no more references
  inline void TryDelete(std::unique_ptr<std::scoped_lock<std::mutex>>& guard) {
    if (!refCountCom && !refCountSharedPtr) {
      // reference count has been verified to be zero,
      // mutex is not valid after call to delete; destruct guard now!
      guard.reset();
      delete this;
    }
  }

  inline void assertAddReference(std::scoped_lock<std::mutex> const& guard) {
    if (refStarted)
      assert((refCountCom + refCountSharedPtr) >
             0); // why aren't we deleted yet?
    else
      assert((refCountCom + refCountSharedPtr) ==
             0); // there should be no references yet
  }
};

} // namespace pi

#endif // PI_H
