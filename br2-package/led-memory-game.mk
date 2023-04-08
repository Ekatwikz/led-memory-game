################################################################################
#
# led-memory-game
#
################################################################################

# Reference: https://buildroot.org/downloads/manual/manual.html#_infrastructure_for_packages_with_specific_build_systems

LED_MEMORY_GAME_VERSION = 0.0.1
LED_MEMORY_GAME_SOURCE = led-memory-game-$(LED_MEMORY_GAME_VERSION).tar.gz
LED_MEMORY_GAME_SITE = https://spages.mini.pw.edu.pl/~katwikirizee/LinES
LED_MEMORY_GAME_LICENSE = GPL-3.0+
LED_MEMORY_GAME_LICENSE_FILES = LICENSE
LED_MEMORY_GAME_DEPENDENCIES = libgpiod

# optimize for debugging experience
# BR passes -Os in CFLAGS/FFLAGS, which ain't ideal for debugging
ifeq ($(BR2_PACKAGE_LED_MEMORY_GAME_DEBUG), y)
	DEBUGFLAGS = "-D 'DEBUG_MODE=1' -Og"
else
	DEBUGFLAGS = "-D 'DEBUG_MODE=0'"
endif

define LED_MEMORY_GAME_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all DEBUGFLAGS=$(DEBUGFLAGS)
endef

define LED_MEMORY_GAME_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/led-memory-game $(TARGET_DIR)/usr/bin/led-memory-game
	$(INSTALL) -D -m 0755 $(@D)/led-memory-game-update $(TARGET_DIR)/usr/bin/led-memory-game-update

	if [ $(BR2_PACKAGE_LED_MEMORY_GAME_DEBUG) = y ]; then\
		$(INSTALL) -D -m 0755 $(@D)/led-memory-game-debug $(TARGET_DIR)/usr/bin/led-memory-game-debug;\
		$(INSTALL) -D -m 0755 $(@D)/led-memory-game-cross-debug $(HOST_DIR)/usr/bin/led-memory-game-cross-debug;\
		$(INSTALL) -D -m 0644 $(@D)/led-memory-game-debugsetup.gdbinit $(HOST_DIR)/etc/led-memory-game-debugsetup.gdbinit;\
	fi
endef

$(eval $(generic-package))

