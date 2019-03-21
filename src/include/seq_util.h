#pragma once
#include <algorithm>
#include <atomic>

#define UNUSED(expr) do { (void)(expr); } while (0)

#define ARR_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DELETE_PTR(x) do { delete x; x = NULL; } while (0);

#define STL_CONTAINS(x, y) std::find(x.begin(), x.end(), y) != x.end()
inline void* aligned_malloc(size_t size, size_t align) {
    void *result;
    #if defined(_MSC_VER) or defined(__MINGW32__)
    result = _aligned_malloc(size, align);
    #else
     if(posix_memalign(&result, align, size)) result = 0;
    #endif
    return result;
}

inline void aligned_free(void *ptr) {
    #if defined(_MSC_VER) or defined(__MINGW32__)
        _aligned_free(ptr);
    #else
      free(ptr);
    #endif

}

template<typename Container>
Container&& sort_unique_erase( Container&& c ) {
  using std::begin; using std::end;
  std::sort( begin(c), end(c) );
  c.erase( std::unique( begin(c), end(c) ), end(c) );
  return std::forward<Container>(c);
}
template<typename C1, typename C2>
C1&& append( C1&& c1, C2&& c2 ) {
  using std::begin; using std::end;
  c1.insert( end(c1), std::make_move_iterator( begin(c2) ), std::make_move_iterator( end(c2) ) );
  return std::forward<C1>(c1);
}
template<typename C1, typename C2>
C1&& append( C1&& c1, C2& c2 ) {
  using std::begin; using std::end;
  c1.insert( end(c1), begin(c2), end(c2) );
  return std::forward<C1>(c1);
}

template<typename C1>
bool any_duplicates(const C1& c) {
	C1 copy = c;
    std::sort(begin(copy), end(copy));
	auto dupe = std::adjacent_find(copy.begin(), copy.end());
	return dupe != copy.end();
}

template<typename C1, typename V1>
  inline bool
  stl_contains(const C1& c, const V1& v){
	return std::find(begin(c), end(c), v) != end(c);
}
template<typename C1>
  inline void
  removeAll(C1& a, const C1& b){
	a.erase( std::remove_if( begin(a),end(a),
	    [&b](auto x){return std::find(begin(b),end(b),x) != end(b);}), end(a) );
}
template<typename C1>
  inline void
  addAll(C1& a, const C1& b){
	a.insert(a.end(), begin(b), end(b));
}
template<typename C1, typename V1>
inline bool removeEntry(C1& a, const V1& b) {
  auto curSize = a.size();
  auto it = a.erase(std::remove_if(a.begin(), itEnd, [b](const auto& ref) {
	  return ref == b;
  }), a.end());
  return curSize != a.size();
}
template<typename C1, typename V1>
  inline int32_t
  indexOfCtr(C1& a, const V1& b){
	auto it = std::find(a.begin(), a.end(), b);
	if (it != a.end()) {
		return static_cast<int32_t>(it - a.begin());
	}
	return -1;
}
template <typename T, typename U>
    inline bool FitsTypeRange(const U value) {
        return value >= std::numeric_limits<T>::min()  && value <= std::numeric_limits<T>::max() ;
    }
template<typename T>
void update_maximum(std::atomic<T>& maximum_value, T const& value) noexcept
{
    T prev_value = maximum_value;
    while(prev_value < value &&
            !maximum_value.compare_exchange_weak(prev_value, value))
        ;
}
