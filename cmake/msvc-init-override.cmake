if (MSVC)
    get_cmake_property(_varNames VARIABLES)
    list (REMOVE_DUPLICATES _varNames)
    list (SORT _varNames)
    foreach (_varName ${_varNames})
        if (_varName MATCHES "CMAKE_C([X][X])?_FLAGS.*_INIT$")
            # remove default warning level from CMAKE_CXX_FLAGS_INIT
            string (REGEX REPLACE "/W[0-4]" "" ${_varName} "${${_varName}}")
            # string (REGEX REPLACE "/O[0-3]" "" ${_varName} "${${_var
        endif()
    endforeach()
endif()