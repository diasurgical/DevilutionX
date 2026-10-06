# Stub out the Threads package.
# Some platforms do not have a system threads library but SDL threads are supported.
set(Threads_FOUND TRUE)
if(NOT TARGET Threads::Threads)
  add_library(Threads::Threads INTERFACE IMPORTED GLOBAL)
endif()
