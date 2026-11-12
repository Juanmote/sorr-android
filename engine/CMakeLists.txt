cmake_minimum_required(VERSION 3.4)


project(sorr C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast")


# Require tre
add_subdirectory(3rdparty/tre)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if(NOT ANDROID)
find_package(SDL2 2.0.4 REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
endif()

# Preprocessor macros
include(common_defs)

set(BGDRTM_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/copy.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/dcbr.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/dirs.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/instance.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/interpreter.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/misc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/strings.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/sysprocs.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/varspace_file.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/src/fmath.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/common/debug.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/common/files.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/common/xctype.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libbgload/bgload.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_say/mod_say.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_string/mod_string.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_math/mod_math.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_mem/mod_mem.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_time/mod_time.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_file/mod_file.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_sound/mod_sound.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libsdlhandler/libsdlhandler.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libjoy/libjoy.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_joy/mod_joy.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libkey/libkey.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_key/mod_key.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_proc/mod_proc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_sort/mod_sort.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_timers/mod_timers.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/g_bitmap.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/g_clear.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/g_grlib.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/g_pal.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/g_blendop.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/g_conversion.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/libgrbase.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libblit/g_blit.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libblit/g_pixel.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libblit/libblit.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libvideo/g_regions.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libvideo/g_video.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/g_fade.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/g_frame.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/g_instance.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/g_object.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/g_rects.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/g_screen.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/librender.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_video/mod_video.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libmouse/libmouse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_multi/mod_multi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/file_fnt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/file_fpg.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/file_map.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/file_pal.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/file_pcx.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/file_png.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/mod_map.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libfont/libfont.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_dir/mod_dir.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libtext/libtext.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_text/mod_text.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_rand/mod_rand.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_grproc/mod_grproc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libscroll/libscroll.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_scroll/mod_scroll.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libdraw/libdraw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_draw/mod_draw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_regex/mod_regex.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_screen/mod_screen.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_path/mod_path.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_effects/mod_effects.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_blendop/mod_blendop.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_m7/mod_m7.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libwm/libwm.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_wm/mod_wm.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_sys/mod_sys.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_flic/mod_flic.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/net.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/inet.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/inet_code.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/sorrnet.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/guest.c
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/miniz.c
  ${CMAKE_CURRENT_SOURCE_DIR}/lowmem/lowmem.c
  ${CMAKE_CURRENT_SOURCE_DIR}/lowmem/tlsf.c)


add_library(bgdrtm STATIC ${BGDRTM_SOURCE_FILES})
target_compile_definitions(bgdrtm PUBLIC -D__BGDRTM__)
target_include_directories(bgdrtm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lowmem)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_definitions(-DBGD_LOWMEM)
  set(LOWMEM_LINK "-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup")
endif()

target_include_directories(bgdrtm PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libbgload/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_say/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_string/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_math/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_mem/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_time/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_file/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_sound/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libsdlhandler/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_joy/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libjoy/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libkey/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_key/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_proc/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_sort/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_timers/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libgrbase/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libblit/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libvideo/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/librender/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_video/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libmouse/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_mouse/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_map/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libfont/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_dir/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libtext/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_text/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_rand/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_grproc/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libscroll/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_scroll/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libdraw/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_draw/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_screen/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_path/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_effects/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_blendop/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_m7/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libwm/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_wm/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_sys/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_flic/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_regex/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_multi/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_chipmunk/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/mod_iap/
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet/
                    ${SDL2_INCLUDE_DIR}
                    ${SDL2_MIXER_INCLUDE_DIRS}
                    ${ZLIB_INCLUDE_DIRS}
                    ${VORBIS_INCLUDE_DIR}
                    ${TRE_INCLUDE_DIR}
                    ${PNG_INCLUDE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tre/lib)


# Source files
set(BGDI_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdi/src/main.c
)

# Include dirs

set(EXTRA_LIBS "-lm ${LOWMEM_LINK}")

# Main executable / Android shared library
if(ANDROID)
  add_library(main SHARED ${BGDI_SOURCE_FILES})
  set(BGDI_TARGET main)
else()
  add_executable(${PROJECT_NAME} ${BGDI_SOURCE_FILES})
  set(BGDI_TARGET ${PROJECT_NAME})
endif()
target_compile_definitions(${BGDI_TARGET} PUBLIC -D__BGDI__)
target_include_directories(${BGDI_TARGET} PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdi/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/libnet
                    ${SDL2_INCLUDE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/core/bgdrtm/include/)

include(compiler_flags)

if(ANDROID)
  target_link_libraries(main
                        bgdrtm
                        SDL2_mixer::SDL2_mixer-static
                        SDL2::SDL2
                        png_static
                        libtre
                        z m log android
                        ${LOWMEM_LINK})
else()
  target_link_libraries(${PROJECT_NAME}
                        bgdrtm
                        ${SDL2_MIXER_LIBRARIES}
                        SDL2::SDL2
                        ${PNG_LIBRARIES}
                        libtre
                        ${ZLIB_LIBRARIES}
                        ${EXTRA_LIBS})
endif()
