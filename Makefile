CC=gcc
CFLAGS=-Wall -pthread

SRC=src/main.c src/buffer.c src/producer.c src/consumer.c
OUT=producer_consumer

all:
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)
