/* NOTE: Providing the full content for CMakeLists.txt for mqtt */
"idf_component_register(
    COMPONENT_SRCS \"mqtt.c\"
    COMPONENT_HEADERS \"mqtt.h\"
    INCLUDE_DIRS \"${PROJECT_SOURCE_DIR}/components/mqtt\"
    REQUIRES \"esp_mqtt\"
)"
