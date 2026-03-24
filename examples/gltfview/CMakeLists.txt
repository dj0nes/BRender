project(gltfview)

add_executable(gltfview
        main.c
        gltf_to_br.c
        gltf_to_br.h
        camera_fps.c
        camera_fps.h
        ground_sky.c
        ground_sky.h
        vendor_impl.c
)

# cgltf.h lives in the vendor directory, stb_image.h in core/fmt
target_include_directories(gltfview PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/../vendor/cgltf
        ${CMAKE_SOURCE_DIR}/core/fmt
)

target_compile_definitions(gltfview PRIVATE GS_TEXTURE_RGB888)
target_link_libraries(gltfview PRIVATE BRender::Full)
set_property(TARGET gltfview PROPERTY FOLDER "BRender/Examples")
set_property(TARGET gltfview PROPERTY C_STANDARD 99)

if(NOT BRENDER_DISABLE_INSTALL)
        install(TARGETS gltfview DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
