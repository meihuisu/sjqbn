# Autoconf/automake file

# General compiler/linker flags
AM_CFLAGS = ${CFLAGS}
AM_FCFLAGS = ${FCFLAGS}
AM_LDFLAGS = ${LDFLAGS}

all:
if WITH_SJQBN_LARGEDATA_DIR
	echo "USING WITH_SJQBN_LARGEDATA_DIR in sjqbn"
else
	echo "NOT USING WITH_SJQBN_LARGEDATA_DIR in sjqbn"
	if [ ! -d "sjqbn" ]; then \
		./make_data_files.py; \
	fi 
endif

install:
if WITH_SJQBN_LARGEDATA_DIR
	if [ ! -d "${prefix}/data/sjqbn" ]; then \
		$(MKDIR_P) ${prefix}/data; \
		$(LN_S) -f $(SJQBN_LARGEDATA_DIR) ${prefix}/data/sjqbn; \
		echo "USING WITH_SJQBN_LARGEDATA_DIR in sjqbn"; \
	fi
	cp config ${prefix}/data
else
	if [ ! -d "${prefix}/data/sjqbn" ]; then \
		$(MKDIR_P) ${prefix}/data; \
		mv sjqbn ${prefix}/data; \
		$(LN_S) -f ${prefix}/data/sjqbn .; \
		echo "NOT USING WITH_SJQBN_LARGEDATA_DIR in sjqbn"; \
	fi
	cp config ${prefix}/data
endif

clean:
	echo "clean"
