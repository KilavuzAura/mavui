set(QGC_APP_NAME "MavUI" CACHE STRING "App Name" FORCE)
set(QGC_ORG_NAME "KilavuzAura" CACHE STRING "Org Name" FORCE)
set(QGC_APP_DESCRIPTION "AURA AUV Ground Control Station" CACHE STRING "Description" FORCE)

# The two strings the upstream defaults leak into every built artifact. The copyright is
# what Windows shows under Properties -> Details, the CPack/IFW publisher field and the
# macOS bundle, so leaving it at the default made a MavUI build introduce itself as
# QGroundControl there no matter what QGC_APP_NAME said. The domain ends up in
# QCoreApplication::setOrganizationDomain, which is the macOS settings path.
set(QGC_APP_COPYRIGHT "Copyright (c) 2026 KilavuzAura. All rights reserved." CACHE STRING "Copyright" FORCE)
set(QGC_ORG_DOMAIN "aurateam.com.tr" CACHE STRING "Domain" FORCE)
# Matches QGC_PACKAGE_NAME below; the default is org.qgroundcontrol.QGroundControl, which
# would make macOS treat a MavUI bundle as an install of QGC.
set(QGC_MACOS_BUNDLE_ID "tr.com.aurateam.mavui" CACHE STRING "MacOS Bundle ID" FORCE)

# Always brand as a stable release — never append " Daily" to the app name
set(QGC_STABLE_BUILD ON CACHE BOOL "Stable Build" FORCE)

# Own package identity so MavUI installs alongside (not on top of) stock QGC.
# Java sources stay in org.mavlink.qgroundcontrol — JNI class paths are unaffected.
set(QGC_PACKAGE_NAME "tr.com.aurateam.mavui" CACHE STRING "Package Name" FORCE)
set(QGC_ANDROID_PACKAGE_NAME "tr.com.aurateam.mavui" CACHE STRING "Android Package Name" FORCE)

set(QGC_MACOS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res" CACHE PATH "MacOS Icon Path" FORCE)
set(QGC_APPIMAGE_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res/icons/MavUI.png" CACHE FILEPATH "AppImage Icon Path" FORCE)

if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp)
    set(QGC_WINDOWS_INSTALL_HEADER_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp" CACHE FILEPATH "Windows Install Header Path" FORCE)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/MavUI.ico)
    set(QGC_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/MavUI.ico" CACHE FILEPATH "Windows Icon Path" FORCE)
endif()

# The .rc compiled into the executable. The stock one is named QGroundControl.rc and
# hardcodes "./WindowsQGC.ico"; it only ever picked up the AURA icon because the custom
# icon happened to share the stock filename. Both are named for this build now, so the
# link between the two is explicit instead of accidental.
if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/MavUI.rc)
    set(QGC_WINDOWS_RESOURCE_FILE_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/MavUI.rc" CACHE FILEPATH "Windows Resource File Path" FORCE)
endif()
