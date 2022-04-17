if (MSVC)
  set(MSVC_WARNINGS_DEBUG 
    /W4
    # Disabled warnings
    /wd4324 # structure was padded due to alignment requirements
    /wd4267 # size_t to int
    /wd4244 # float to int
    /wd4100 # unreferenced formal parameter
    /wd4458 # declaration of x hides class member
    # Enabled warnings not covered by /W4
    /w14263 # 'function': member function does not override any base class virtual member function
    /w14265 # 'classname': class has virtual functions, but destructor is not virtual instances of this class may not
            # be destructed correctly
    /w14287 # 'operator': unsigned/negative constant mismatch
    /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside
            # the for-loop scope
    /w14296 # 'operator': expression is always 'boolean_value'
    /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
    /w14545 # expression before comma evaluates to a function which is missing an argument list
    /w14546 # function call before comma missing argument list
    /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
    /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
    /w14555 # expression has no effect; expected expression with side- effect
    /w14619 # pragma warning: there is no warning number 'number'
    /w14640 # Enable warning on thread un-safe static member initialization
    /w14826 # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
    /w14905 # wide string literal cast to 'LPSTR'
    /w14906 # string literal cast to 'LPWSTR'
    /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
    /permissive- # standards conformance mode for MSVC compiler.
  )
  set(MSVC_WARNINGS_RELEASE /W3 /wd4267 /wd4244 /wd4100 /wd4458 /wd4305)
  set(BUILD_WARNING_FLAGS $<IF:$<CONFIG:Debug>,${MSVC_WARNINGS_DEBUG},${MSVC_WARNINGS_RELEASE}>)
else ()
  set(BUILD_WARNING_FLAGS -Wall -Wno-unused-parameter)
  if (NOT CLANG)
    # GCC warns in cases like 'if (x_s32 < 0) return; if (x_s32 >= len_u32) x_s32 = len_u32 - 1;'
    list(APPEND BUILD_WARNING_FLAGS -Wno-sign-compare)
  endif()
endif ()