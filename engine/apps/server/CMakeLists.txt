set(PROTO_FILE ${CMAKE_SOURCE_DIR}/proto/risk_engine.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS ${GENERATED_DIR}/risk_engine.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/risk_engine.pb.h)
set(GRPC_SRCS ${GENERATED_DIR}/risk_engine.grpc.pb.cc)
set(GRPC_HDRS ${GENERATED_DIR}/risk_engine.grpc.pb.h)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         -I ${CMAKE_SOURCE_DIR}/proto
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating C++ protobuf/gRPC code from risk_engine.proto"
)

add_library(risk_engine_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS})
target_include_directories(risk_engine_proto PUBLIC ${GENERATED_DIR})
target_link_libraries(risk_engine_proto PUBLIC gRPC::grpc++ protobuf::libprotobuf)

add_executable(risk_engine_server main.cpp)
target_link_libraries(risk_engine_server PRIVATE engine_core risk_engine_proto)

add_executable(risk_engine_client_smoketest client_smoketest.cpp)
target_link_libraries(risk_engine_client_smoketest PRIVATE risk_engine_proto)
