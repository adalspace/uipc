add_executable(server src/server.cpp)
add_executable(packet_test src/packet.cpp)
add_executable(client src/client.cpp)

target_compile_features(server
    PRIVATE
        cxx_std_23
)

target_compile_features(packet_test
    PRIVATE
        cxx_std_23
)

target_compile_features(client
    PRIVATE
        cxx_std_23
)

target_link_libraries(server PRIVATE uipc::cpp)
target_link_libraries(client PRIVATE uipc::cpp)
target_link_libraries(packet_test PRIVATE uipc::cpp)
