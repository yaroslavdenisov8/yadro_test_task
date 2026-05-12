add_executable(node_b
    main.cpp
    node_b.cpp
    ${GENERAL_CPP}
)

target_include_directories(node_b PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERAL_DIR}
)

target_link_libraries(node_b PRIVATE
    accel_proto
    Threads::Threads
    yaml-cpp
    spdlog::spdlog
)

add_custom_command(TARGET node_b POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{HOME}/.config/systemd/user/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/accel_node_b.service
            $ENV{HOME}/.config/systemd/user/accel_node_b.service
)

add_dependencies(node_b generate_certs)
