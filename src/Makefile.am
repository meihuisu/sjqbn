# Autoconf/automake file

objects = um_netcdf.o sjqbn_util.o cJSON.o

# General compiler/linker flags
AM_CFLAGS = ${CFLAGS} ${CPPFLAGS} -I$(prefix)/include
AM_LDFLAGS = ${LDFLAGS} -L$(prefix)/lib ${LIBS} -lm


TARGETS = sjqbn_query libsjqbn.a libsjqbn.so

all: $(TARGETS)

install:
	mkdir -p ${prefix}
	mkdir -p ${prefix}/lib
	mkdir -p ${prefix}/include
	cp libsjqbn.so ${prefix}/lib
	cp libsjqbn.a ${prefix}/lib
	cp sjqbn.h ${prefix}/include
	cp sjqbn_query ${prefix}/bin

clean:
	rm -rf $(TARGETS)
	rm -rf *.o

libsjqbn.a: sjqbn_static.o sjqbn_util.o um_netcdf.o cJSON.o
	$(AR) rcs $@ $^

libsjqbn.so: sjqbn.o sjqbn_util.o  um_netcdf.o cJSON.o
	$(CC) -shared $(AM_FCFLAGS) -o libsjqbn.so $^ $(AM_LDFLAGS)

sjqbn.o: sjqbn.c
	$(CC) -fPIC -DDYNAMIC_LIBRARY -o $@ -c $^ $(AM_CFLAGS)

sjqbn_static.o: sjqbn.c
	$(CC) -o $@ -c $^ $(AM_CFLAGS)

$(objects): %.o: %.c
	$(CC) -fPIC -DDYNAMIC_LIBRARY $(AM_CFLAGS) -o $@ -c $^

sjqbn_query.o: sjqbn_query.c
	$(CC) $(AM_CFLAGS) -o $@ -c $^

sjqbn_query : sjqbn_query.o libsjqbn.a
	$(CC) -o $@ $^ $(AM_LDFLAGS)


