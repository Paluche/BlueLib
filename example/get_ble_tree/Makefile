#  BlueLib - Abstraction layer for Bluetooth Low Energy softwares
#
#  Copyright (C) 2013  Netatmo
#  Copyright (C) 2014  Hubert Lefevre
#
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,

CFLAGS += -I../../include
CFLAGS += -I../../bluez
CFLAGS += $(shell pkg-config --cflags glib-2.0)
CFLAGS += -std=gnu99
CFLAGS += -Wall

LDLIBS += $(shell pkg-config --libs glib-2.0)
LDLIBS += -lbluetooth

.PHONY: clean distclean all

all: get_ble_tree

get_ble_tree: get_ble_tree.o \
              bluelib.o bluelib_gatt.o callback.o conn_state.o notif.o \
			  att.o btio.o gatt.o gattrib.o utils.o uuid.o

%.o: ../../src/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

%.o: ../../bluez/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	-rm -f *.o

distclean: clean
	-rm -f get_ble_tree

include ../../ble.mk
