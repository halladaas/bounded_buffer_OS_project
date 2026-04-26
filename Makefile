CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
LDFLAGS = -lpthread

SRC     = src/main.c src/buffer.c src/producer.c src/consumer.c src/utils.c
OBJ     = $(SRC:.c=.o)
TARGET  = producer_consumer

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET) results/metrics.txt
