# =============================================================================
# MeyerScanScanThirdParty.cmake
#
# Purpose:
#   Centralize Qt/VTK/OpenCV lookup rules for ScanReconstructStudio-related
#   modules. VS2015 projects use MeyerScanScanThirdParty.props; CMake projects
#   use this file so path rules stay consistent.
#
# Portability:
#   1. Prefer environment variables when the project is copied to another PC.
#   2. Then try a repository-local ThirdParty directory.
#   3. Finally fall back to the current development machine paths used by the
#      reference SelectPolyData project.
# =============================================================================

if(NOT DEFINED MEYER_ROOT_DIR)
    get_filename_component(MEYER_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

if(NOT MEYER_SCAN_VTK_ROOT AND DEFINED ENV{VTK_ROOT})
    set(MEYER_SCAN_VTK_ROOT "$ENV{VTK_ROOT}" CACHE PATH "VTK 8.0 install root" FORCE)
endif()
if(NOT MEYER_SCAN_VTK_ROOT AND EXISTS "${MEYER_ROOT_DIR}/ThirdParty/VTK/VTK8_Release/lib/cmake/vtk-8.0/VTKConfig.cmake")
    set(MEYER_SCAN_VTK_ROOT "${MEYER_ROOT_DIR}/ThirdParty/VTK/VTK8_Release" CACHE PATH "VTK 8.0 install root" FORCE)
endif()
if(NOT MEYER_SCAN_VTK_ROOT AND EXISTS "D:/wj/SelectPolyData/VTK/VTK8_Release/lib/cmake/vtk-8.0/VTKConfig.cmake")
    set(MEYER_SCAN_VTK_ROOT "D:/wj/SelectPolyData/VTK/VTK8_Release" CACHE PATH "VTK 8.0 install root" FORCE)
endif()

if(NOT MEYER_SCAN_VTK_HEADERS_ROOT AND DEFINED ENV{VTK_HEADERS_ROOT})
    set(MEYER_SCAN_VTK_HEADERS_ROOT "$ENV{VTK_HEADERS_ROOT}" CACHE PATH "VTK generated/source headers root" FORCE)
endif()
if(NOT MEYER_SCAN_VTK_HEADERS_ROOT AND EXISTS "${MEYER_ROOT_DIR}/ThirdParty/VTKHeaders/include/vtk-8.0/vtkPolyData.h")
    set(MEYER_SCAN_VTK_HEADERS_ROOT "${MEYER_ROOT_DIR}/ThirdParty/VTKHeaders" CACHE PATH "VTK generated/source headers root" FORCE)
endif()
if(NOT MEYER_SCAN_VTK_HEADERS_ROOT AND EXISTS "D:/wj/SelectPolyData/VTKHeaders/include/vtk-8.0/vtkPolyData.h")
    set(MEYER_SCAN_VTK_HEADERS_ROOT "D:/wj/SelectPolyData/VTKHeaders" CACHE PATH "VTK generated/source headers root" FORCE)
endif()

if(NOT MEYER_SCAN_OPENCV_ROOT AND DEFINED ENV{OPENCV_ROOT})
    set(MEYER_SCAN_OPENCV_ROOT "$ENV{OPENCV_ROOT}" CACHE PATH "OpenCV 3.3 install root" FORCE)
endif()
if(NOT MEYER_SCAN_OPENCV_ROOT AND EXISTS "${MEYER_ROOT_DIR}/ThirdParty/opencv/build/x64/vc14/lib/OpenCVConfig.cmake")
    set(MEYER_SCAN_OPENCV_ROOT "${MEYER_ROOT_DIR}/ThirdParty/opencv" CACHE PATH "OpenCV 3.3 install root" FORCE)
endif()
if(NOT MEYER_SCAN_OPENCV_ROOT AND EXISTS "D:/SoftWareInstall/opencv/build/x64/vc14/lib/OpenCVConfig.cmake")
    set(MEYER_SCAN_OPENCV_ROOT "D:/SoftWareInstall/opencv" CACHE PATH "OpenCV 3.3 install root" FORCE)
endif()

if(NOT MEYER_SCAN_VTK_ROOT)
    message(FATAL_ERROR
        "VTK 8.0 root not found. Set VTK_ROOT or put VTK under "
        "${MEYER_ROOT_DIR}/ThirdParty/VTK/VTK8_Release.")
endif()
if(NOT MEYER_SCAN_OPENCV_ROOT)
    message(FATAL_ERROR
        "OpenCV 3.3 root not found. Set OPENCV_ROOT or put OpenCV under "
        "${MEYER_ROOT_DIR}/ThirdParty/opencv.")
endif()

set(VTK_DIR "${MEYER_SCAN_VTK_ROOT}/lib/cmake/vtk-8.0" CACHE PATH
    "VTK 8.0 cmake package directory" FORCE)
set(OpenCV_DIR "${MEYER_SCAN_OPENCV_ROOT}/build/x64/vc14/lib" CACHE PATH
    "OpenCV 3.3 vc14 cmake package directory" FORCE)

if(MEYER_SCAN_THIRD_PARTY_RUNTIME_ONLY)
    return()
endif()

find_package(OpenCV REQUIRED)

set(MEYER_SCAN_VTK_REQUIRED_COMPONENTS
    vtkCommonCore
    vtkCommonDataModel
    vtkCommonExecutionModel
    vtkFiltersCore
    vtkFiltersGeneral
    vtkFiltersSources
    vtkGUISupportQt
    vtkIOCore
    vtkIOGeometry
    vtkInteractionStyle
    vtkRenderingCore
    vtkRenderingFreeType
    vtkRenderingOpenGL2
)

find_package(VTK 8.0 REQUIRED COMPONENTS ${MEYER_SCAN_VTK_REQUIRED_COMPONENTS})
include(${VTK_USE_FILE})

# VTK 8 的安装输出目录和生成头目录可能同时存在 QVTKWidget.h。
# SelectPolyData 参考工程中可编译的 QVTKWidget.h 位于 VTKHeaders，
# 因此这里把 VTKHeaders 放到 include 搜索最前面，避免 MSVC 误读
# VTK 安装目录中不可编译的生成/占位头文件。
include_directories(BEFORE "${MEYER_SCAN_VTK_HEADERS_ROOT}/include/vtk-8.0")
