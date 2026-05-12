add_executable(server
    main.cpp
    server.cpp
    ${GENERAL_CPP}
)

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERAL_DIR}
)

target_link_libraries(server PRIVATE
    accel_proto
    Threads::Threads
    yaml-cpp
    spdlog::spdlog
)

add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.config/systemd/user/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/accel_server.service
            $ENV{HOME}/.config/systemd/user/accel_server.service
)

add_dependencies(server generate_certs)
