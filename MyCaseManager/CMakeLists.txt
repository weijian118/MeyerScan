cmake_minimum_required(VERSION 3.15)
project(MeyerScan_CaseManagerReference LANGUAGES NONE)

# MyCaseManager 当前是旧版 mysql.sql / 旧 schema 参考目录，不产生活跃 DLL。
# 保留这个 CMakeLists 是为了满足“每个模块目录都能被 VSCode/CMake 识别”的工程规则。
