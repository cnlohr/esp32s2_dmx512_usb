all : dmx512testapp

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CFLAGS:=-g -O0
LDFLAGS:=-ludev
else
CFLAGS:=-g -O0
LDFLAGS:=C:/windows/system32/setupapi.dll
endif

dmx512testapp : dmx512testapp.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ dmx512testapp

