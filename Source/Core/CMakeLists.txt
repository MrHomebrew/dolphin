add_subdirectory(AudioCommon)
add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(DiscIO)
add_subdirectory(InputCommon)
add_subdirectory(UICommon)
add_subdirectory(VideoCommon)
add_subdirectory(VideoBackends)

if(ENABLE_NOGUI)
  add_subdirectory(DolphinNoGUI)
endif()

if(ENABLE_QT)
  add_subdirectory(DolphinQt)
endif()

<<<<<<< HEAD
if(LIBRETRO)
  add_subdirectory(DolphinLibretro)
else()
  add_subdirectory(DolphinNoGUI)
=======
if (APPLE)
  add_subdirectory(UpdaterCommon)
  add_subdirectory(MacUpdater)
>>>>>>> 1d5dd5db914d94f3f612c13c6c5e1d5e711b49b5
endif()
