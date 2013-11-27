CFLAGS += -I../../include
CFLAGS += -I../../bluez
CFLAGS += $(shell pkg-config --cflags glib-2.0)
CFLAGS += -std=gnu99
CFLAGS += -Wall

LDLIBS += $(shell pkg-config --libs glib-2.0)
LDLIBS += -lbluetooth

all: get_ble_tree

get_ble_tree: get_ble_tree.o \
              bluelib.o bluelib_gatt.o callback.o conn_state.o notif.o \
							att.o btio.o gatt.o gattrib.o utils.o

%.o: ../../src/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

%.o: ../../bluez/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	-rm -f *.o

distclean: clean
	-rm -f get_ble_tree
