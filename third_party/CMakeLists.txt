# linenoise from github.com/antirez/linenoise at a473823d74b93eab2ba83480df16ed37617493f2,
# unmodified (BSD 2-clause). Not built with our warning flags
add_library(linenoise STATIC linenoise/linenoise.c)
target_include_directories(linenoise SYSTEM PUBLIC linenoise)
set_target_properties(linenoise PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
