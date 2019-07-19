set(mountservicemanager_SRCS mountservicemanager.cpp)

find_package(KDED CONFIG REQUIRED)

qt5_add_dbus_adaptor(mountservicemanager_SRCS org.kde.kio.MountServiceManager.xml mountservicemanager.h MountServiceManager)
qt5_add_dbus_interface(mountservicemanager_SRCS ${KDED_DBUS_INTERFACE} kded_interface)

add_library(mountservicemanager MODULE ${mountservicemanager_SRCS})

kcoreaddons_desktop_to_json(mountservicemanager mountservicemanager.desktop)

target_link_libraries(mountservicemanager
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::KIOCore
)

install(TARGETS mountservicemanager DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kded)
install(FILES org.kde.kio.MountServiceManager.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
