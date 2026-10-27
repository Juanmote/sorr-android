# Bump up warning levels appropriately for clang, gcc & msvc
# Also set debug/optimization flags depending on the build type. IDE users choose this when
# selecting the build mode in their IDE
# set(CMAKE_VERBOSE_MAKEFILE ON)
if (${CMAKE_C_COMPILER_ID} MATCHES "GNU" OR
    ${CMAKE_C_COMPILER_ID} MATCHES "Clang" OR
    ${CMAKE_C_COMPILER_ID} MATCHES "AppleClang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -fno-strict-aliasing -fsigned-char")
        if (${CMAKE_C_COMPILER_ID} MATCHES "Clang")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-incompatible-function-pointer-types -Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast")
        endif()
        set_target_properties(${BGDI_TARGET} PROPERTIES COMPILE_FLAGS "-fsigned-char")
elseif (MSVC)
        # The following flags disable warnings that suggest to use MSVC-specific functions
        set(CMAKE_C_FLAGS "/wd4996 ${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
        if (CMAKE_C_FLAGS MATCHES "/W[0-4]")
                string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        else()
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
        endif()
endif()
