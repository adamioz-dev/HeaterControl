
    // AUTO GENERATED FILE, DO NOT EDIT
    #include "AVRHexArray.h" // get uno version stored in the hex array "VERSION_UNO"

    #ifndef PROJECT_MAJOR
        #define PROJECT_MAJOR 1
    #endif
    #ifndef PROJECT_UNO
        #define PROJECT_UNO VERSION_UNO
    #endif
    #ifndef PROJECT_ESP
        #define PROJECT_ESP 31
    #endif

    #ifndef PROJECT_STR_VERSION
        // Create version inforamtion in str format
        #define VERS(version)               #version
        #define VERSION_STR(major,uno,esp)  VERS(major) "." VERS(uno) "." VERS(esp)
        #define PROJECT_STR_VERSION         VERSION_STR(PROJECT_MAJOR, PROJECT_UNO, PROJECT_ESP)
    #endif //PROJECT_STR_VERSION

    #ifndef PROJECT_BUILD_TIMESTAMP
        #define PROJECT_BUILD_TIMESTAMP "2025-09-17 16:12:48.325469"
    #endif
    