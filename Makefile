# Compiler to use
CC = arm-linux-gnueabihf-gcc

# Compiler flags
CFLAGS = -Wall 

# The name of the final executable
TARGET =cross_program

# Source files
SRCS =main.c 

# Object files
OBJS =main.o

#/build/include
CFLAGS = -Wall -I/home/geoth/rasp_libraries/openssl-1.1.1t/build/include -I/home/geoth/rasp_libraries/libwebsockets/build/include -I/home/geoth/rasp_libraries/cJSON-1.7.18/build/include -I/home/geoth/rasp_libraries/zlib/build/include -O2
#/build/lib
LDFLAGS = -L/home/geoth/rasp_libraries/openssl-1.1.1t/build/lib -L/home/geoth/rasp_libraries/libwebsockets/build/lib -L/home/geoth/rasp_libraries/cJSON-1.7.18/build/lib -L/home/geoth/rasp_libraries/zlib/build/lib -lwebsockets -lcjson -lpthread -lssl -lcrypto



# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
# Clean up generated files
clean:
	rm -f $(TARGET) $(OBJS)
 
