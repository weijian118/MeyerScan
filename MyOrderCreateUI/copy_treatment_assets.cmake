# 复制建单治疗方案资源。
#
# 单模块构建时，资源源目录和目标目录都可能是 MyOrderCreateUI/bin/Release/icon/createModule/sacanPlan。
# CMake 的 copy_directory 不允许把目录复制到自身，所以这里先比较绝对路径，相同时直接跳过。
get_filename_component(source_abs "${source_dir}" ABSOLUTE)
get_filename_component(target_abs "${target_dir}" ABSOLUTE)

if(NOT source_abs STREQUAL target_abs)
    file(MAKE_DIRECTORY "${target_abs}")
    file(COPY "${source_abs}/" DESTINATION "${target_abs}")
endif()
