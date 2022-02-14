
#include <string_view>

namespace nameoftype {
#if defined(__clang__) && __clang_major__ >= 5 || defined(__GNUC__) && __GNUC__ >= 7 || defined(_MSC_VER) && _MSC_VER >= 1910
#else
# error "nameoftype supported by compiler"
#endif

using std::string_view;

#if defined(_MSC_VER) && !defined(__clang__)
template <typename T>
struct identity {
  using type = T;
};
#else
template <typename T>
using identity = T;
#endif

template <typename... T>
constexpr auto get_name() noexcept {
#if defined(__clang__)
  return string_view{__PRETTY_FUNCTION__ + 34, sizeof(__PRETTY_FUNCTION__) - 39};
#elif defined(__GNUC__)
  return string_view{__PRETTY_FUNCTION__ + 49, sizeof(__PRETTY_FUNCTION__) - 52};
#elif defined(_MSC_VER)
  return string_view{__FUNCSIG__ + 34, sizeof(__FUNCSIG__)-51};
#endif
}

template <typename T>
constexpr auto get_name_short() noexcept {
  return get_name<std::remove_cv_t<std::remove_reference_t<T>>>();
}

constexpr string_view remove_prefix(string_view name) noexcept {
  if (name.size() > 7) {
    using namespace std::literals::string_view_literals;
    constexpr auto classPrefix = "class "sv;
    constexpr auto structPrefix = "struct "sv;
    if (name.substr(0, classPrefix.size()).compare(classPrefix) == 0) {
      name.remove_prefix(classPrefix.size());
      return name;
    }
    if (name.substr(0, structPrefix.size()).compare(structPrefix) == 0) {
      name.remove_prefix(structPrefix.size());
      return name;
    }
  }
  return name;
}
}

#define NAMEOFOBJ(...) ::nameoftype::remove_prefix(::nameoftype::get_name_short<decltype(__VA_ARGS__)>())
#define NAMEOFTYPE(...) ::nameoftype::remove_prefix(::nameoftype::get_name_short<__VA_ARGS__>())