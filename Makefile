lib=allocator.so

# Set the following to '0' to disable log messages:
LOGGER ?=0

CFLAGS += -Wall -g -pthread -fPIC -shared
LDFLAGS +=

$(lib): allocator.c allocator.h logger.h 
	$(CC) $(CFLAGS) $(LDFLAGS) -DLOGGER=$(LOGGER) allocator.c -o $@

docs: Doxyfile
	doxygen

clean:
	rm -f $(lib)
	rm -rf docs


