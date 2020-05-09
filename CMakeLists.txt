cmake_minimum_required(VERSION 3.0)
project(minipro)

set(CMAKE_CXX_STANDARD 14)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Log4cxx REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_search_module(GLIB REQUIRED glib-2.0)

include_directories(include ${GLIB_INCLUDE_DIRS})

add_library(bluez STATIC
  lib/bluez/att.c
  lib/bluez/bluetooth.c
  lib/bluez/crypto.c
  lib/bluez/gatt-client.c
  lib/bluez/gatt-db.c
  lib/bluez/gatt-helpers.c
  lib/bluez/hci.c
  lib/bluez/io-mainloop.c
  lib/bluez/mainloop.c
  lib/bluez/queue.c
  lib/bluez/timeout-glib.c
  lib/bluez/util.c
  lib/bluez/uuid.c
)

add_executable(gattclient ${BLUEZ_SRC} lib/bluez/btgattclient.c)
target_link_libraries(gattclient bluez ${GLIB_LDFLAGS})
target_include_directories(gattclient PUBLIC lib/bluez)

add_executable(minipro ${BLUEZ_SRC}
  src/minipro/main.cpp 
  src/minipro/minipro.cpp
  src/bluetooth/bluetooth_le_device.cpp
  src/bluetooth/utils.cpp
  src/util/joystick.cpp
  src/util/loop_rate.cpp
)
target_link_libraries(minipro bluez ${GLIB_LDFLAGS} pthread)
target_include_directories(minipro PUBLIC lib/bluez src/util)
