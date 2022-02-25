project (ddb)
cmake_minimum_required (VERSION 3.2)

add_library (ddb STATIC
	src/av.cc
	src/error.cc
)

target_link_libraries (ddb PUBLIC
	avcodec
	avfilter
	avutil
	avformat
)

add_executable (ddb-cli src/cli.cc)

target_link_libraries (ddb-cli PUBLIC ddb)

set_target_properties (ddb-cli PROPERTIES RUNTIME_OUTPUT_NAME ddb)

target_compile_features (ddb PRIVATE cxx_std_17)
target_compile_features (ddb-cli PRIVATE cxx_std_17)

target_compile_options (ddb PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)
target_compile_options (ddb-cli PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)
