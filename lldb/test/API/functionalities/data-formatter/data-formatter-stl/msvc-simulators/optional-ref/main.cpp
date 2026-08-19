// Layout approximation of polyfill / Beman Optional26 std::optional<T&>.

namespace std {
template <class T> class optional;
template <class T> class optional<T &> {
public:
  optional() : value_(nullptr) {}
  explicit optional(T &v) : value_(&v) {}
  T *value_;
};
} // namespace std

struct S {
  int a;
};

int main() {
  int x = 42;
  S s{7};
  std::optional<int &> engaged{x};
  std::optional<int &> empty;
  std::optional<S &> engaged_s{s};
  std::optional<S &> empty_s;
  return 0; // break here
}
