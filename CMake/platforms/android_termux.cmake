# Compiling on an Android device itself, e.g. via Termux.
#
# In this scenario, Android is more like a regular Linux platform,
# and the NDK is not available.
set(TERMUX ON)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND DEFINED ENV{PREFIX})
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE $ENV{PREFIX})
endif()
