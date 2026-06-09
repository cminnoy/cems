project(schemas LANGUAGES CXX VERSION 0.1.0 DESCRIPTION "Flatbuffers generation")

link_directories(${CMAKE_BINARY_DIR})

set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS --cpp --python --scoped-enums --gen-mutable)
set(FB_SCHEMAS "master_clock.fbs;dsmr.fbs;circutor.fbs;elkor.fbs;ime.fbs;zendure_control.fbs;zendure_driver.fbs;predictor.fbs;open_meteo.fbs;daytime.fbs")
build_flatbuffers("${FB_SCHEMAS}" "" fbs_schemas "${FB_DEPS}" "${CMAKE_CURRENT_BINARY_DIR}/" "" "")
