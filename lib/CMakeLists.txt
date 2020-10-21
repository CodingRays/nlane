include(FetchContent)

# Download and add googletest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY  "https://github.com/google/googletest.git"
    GIT_TAG         "v1.10.x"
)

FetchContent_MakeAvailable(googletest)