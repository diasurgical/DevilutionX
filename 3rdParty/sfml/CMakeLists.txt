include(functions/FetchContent_ExcludeFromAll_backport)

set(SFML_BUILD_WINDOW OFF)
set(SFML_BUILD_GRAPHICS OFF)
set(SFML_BUILD_AUDIO OFF)

include(FetchContent)
FetchContent_Declare_ExcludeFromAll(sfml
    GIT_REPOSITORY https://github.com/SFML/SFML
    GIT_TAG 016bea9491ccafc3529019fe1d403885a8b3a6ae
)
FetchContent_MakeAvailable_ExcludeFromAll(sfml)