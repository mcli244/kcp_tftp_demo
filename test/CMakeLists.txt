cmake_minimum_required (VERSION 2.8)

project (demo)

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Includes
include_directories (${PROJECT_SOURCE_DIR}
                    ${PROJECT_SOURCE_DIR}/../
                    )

set(KCP_SRC
    ${PROJECT_SOURCE_DIR}/../ikcp.c
)

add_executable(server 
                server.c 
                ${KCP_SRC}
                )
target_link_libraries(server 
                rt
                pthread
                )
                
add_executable(client 
                client.c
                ${KCP_SRC}
                )
target_link_libraries(client 
                rt
                pthread
                )
