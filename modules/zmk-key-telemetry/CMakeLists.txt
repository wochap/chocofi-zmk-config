# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_KEY_TELEMETRY)
  target_include_directories(app PRIVATE include)
  target_sources(app PRIVATE src/protocol.c src/telemetry.c)
endif()
