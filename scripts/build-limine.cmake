file(MAKE_DIRECTORY "${OUTPUT_DIR}")

if (NOT EXISTS "${LIMINE_ARCHIVE}")
    message(STATUS "Limine not found in ${LIMINE_DIR}")
    message(STATUS "Downloading limine ${LIMINE_VERSION} into ${LIMINE_DIR}")
    message(STATUS "Download URL is ${LIMINE_URL}")
    file(DOWNLOAD
        "${LIMINE_URL}"
        "${LIMINE_ARCHIVE}"
        SHOW_PROGRESS
        STATUS LIMINE_DOWNLOAD_STATUS
    )

    list(GET LIMINE_DOWNLOAD_STATUS 0 LIMINE_DOWNLOAD_RESULT)

    if(NOT LIMINE_DOWNLOAD_RESULT EQUAL 0)
        list(GET LIMINE_DOWNLOAD_STATUS 1 LIMINE_DOWNLOAD_ERROR)

        file(REMOVE "${LIMINE_ARCHIVE}")

        message(FATAL_ERROR
            "Failed to download Limine: ${LIMINE_DOWNLOAD_ERROR}"
        )
    endif()
else()
    message(STATUS "Using cached Limine: ${LIMINE_ARCHIVE}")
endif()

if(NOT EXISTS "${LIMINE_DIR}")
    message(STATUS "Extracting Limine")

    file(MAKE_DIRECTORY "${LIMINE_DIR}")

    file(ARCHIVE_EXTRACT
        INPUT "${LIMINE_ARCHIVE}"
        DESTINATION "${OUTPUT_DIR}"
    )

endif()

if(NOT EXISTS "${LIMINE_DIR}/limine")
    message(STATUS "Building Limine")

    execute_process(
        COMMAND make
        WORKING_DIRECTORY "${LIMINE_DIR}"
        RESULT_VARIABLE MAKE_RESULT
    )

    if(NOT MAKE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to build Limine (make result ${MAKE_RESULT})"
        )
    endif()
else()
    message(STATUS "Limine binary already exists: ${LIMINE_DIR}/limine")
endif()
