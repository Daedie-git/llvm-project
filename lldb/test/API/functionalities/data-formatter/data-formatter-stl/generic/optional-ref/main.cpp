#include <optional>
#include <string>

#if defined(__cpp_lib_optional) && __cpp_lib_optional >= 202506L
#define HAVE_OPTIONAL_REF 1
#else
#define HAVE_OPTIONAL_REF 0
#endif

int main() {
  bool has_optional_ref = HAVE_OPTIONAL_REF;
#if HAVE_OPTIONAL_REF
  int x = 42;
  std::string s = "hello";
  std::optional<int &> engaged = x;
  std::optional<int &> empty;
  std::optional<std::string &> engaged_s = s;
  std::optional<std::string &> empty_s;
  (void)engaged;
  (void)empty;
  (void)engaged_s;
  (void)empty_s;
#endif
  return 0; // break here
}
