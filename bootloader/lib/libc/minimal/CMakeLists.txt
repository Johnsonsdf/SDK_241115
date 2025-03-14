# SPDX-License-Identifier: Apache-2.0

zephyr_system_include_directories(include)

zephyr_library()
zephyr_library_sources(
  source/stdlib/abort.c
  source/stdlib/atoi.c
  source/stdlib/strtol.c
  source/stdlib/strtoul.c
  source/stdlib/malloc.c
  source/stdlib/bsearch.c
  source/stdlib/exit.c
  source/string/strncasecmp.c
  source/string/strstr.c
  source/string/strspn.c
  source/stdout/stdout_console.c
  source/stdout/sprintf.c
  source/stdout/fprintf.c
  source/time/gmtime.c
)

if(CONFIG_LIBC_STRING_BROM)
  zephyr_library_sources(source/string/string_brom.c)
else() 
  zephyr_library_sources(source/string/string.c)
  zephyr_library_sources(source/string/string.S)
endif()
zephyr_library_sources_ifdef(CONFIG_POSIX_CLOCK source/time/time.c)
zephyr_library_sources_ifdef(CONFIG_MINIMAL_LIBC_RAND source/stdlib/rand.c)
