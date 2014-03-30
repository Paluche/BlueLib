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

EXE 		 = get_ble_tree
EXE_SRC      = main.c
BLUELIB_SRC	 = bluelib.c bluelib_gatt.c callback.c conn_state.c notif.c
BLUEZ_SRC    = att.c btio.c gatt.c gattrib.c utils.c uuid.c

OBJDIR       = objs
EXE_OBJS     = $(addprefix $(OBJDIR)/, $(notdir $(SRC:.c=.o)))
BLUELIB_OBJS = $(addprefix $(OBJDIR)/, $(notdir $(BLUELIB_SRC:.c=.o)))
BLUEZ_OBJS   = $(addprefix $(OBJDIR)/, $(notdir $(BLUEZ_SRC:.c=.o)))
OBJS         = $(EXE_OBJS) $(BLUELIB_OBJS) $(BLUEZ_OBJS)


.PHONY: clean distclean all
all: $(EXE) $(OBJDIR)

$(EXE): $(OBJS)

$(OBJDIR):
	@mkdir $(OBJDIR)

$(EXE_OBJS): $(OBJDIR)/%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(BLUELIB_OBJS): $(OBJDIR)/%.o: ../../src/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(BLUEZ_OBJS): $(OBJDIR)/%.o: ../../bluez/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	-rm -f $(OBJS)
	-rm -rf $(OBJDIR)
	-rm -f $(EXE)

include ../../ble.mk
