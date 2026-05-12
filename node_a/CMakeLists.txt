add_executable(node_a
    main.cpp
    node_a.cpp
    emulator.cpp
    ${GENERAL_CPP}
)

target_include_directories(node_a PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERAL_DIR}
)

target_link_libraries(node_a PRIVATE
    accel_proto
    Threads::Threads
    yaml-cpp
    spdlog::spdlog
)

add_custom_command(TARGET node_a POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.config/systemd/user/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/accel_node_a.service
            $ENV{HOME}/.config/systemd/user/accel_node_a.service
)

add_dependencies(node_a generate_certs)
