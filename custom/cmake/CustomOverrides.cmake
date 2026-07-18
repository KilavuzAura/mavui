set(QGC_APP_NAME "MavUI" CACHE STRING "App Name" FORCE)
set(QGC_ORG_NAME "KilavuzAura" CACHE STRING "Org Name" FORCE)
set(QGC_APP_DESCRIPTION "AURA AUV Ground Control Station" CACHE STRING "Description" FORCE)

# Always brand as a stable release — never append " Daily" to the app name
set(QGC_STABLE_BUILD ON CACHE BOOL "Stable Build" FORCE)

# Own package identity so MavUI installs alongside (not on top of) stock QGC.
# Java sources stay in org.mavlink.qgroundcontrol — JNI class paths are unaffected.
set(QGC_PACKAGE_NAME "tr.com.aurateam.mavui" CACHE STRING "Package Name" FORCE)
set(QGC_ANDROID_PACKAGE_NAME "tr.com.aurateam.mavui" CACHE STRING "Android Package Name" FORCE)

set(QGC_MACOS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res" CACHE PATH "MacOS Icon Path" FORCE)
set(QGC_APPIMAGE_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res/icons/custom_qgroundcontrol.png" CACHE FILEPATH "AppImage Icon Path" FORCE)

if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp)
    set(QGC_WINDOWS_INSTALL_HEADER_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp" CACHE FILEPATH "Windows Install Header Path" FORCE)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/WindowsQGC.ico)
    set(QGC_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/WindowsQGC.ico" CACHE FILEPATH "Windows Icon Path" FORCE)
endif()
