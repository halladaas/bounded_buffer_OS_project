CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -pthread

SRC     = src/producer_consumer.c
TARGET  = producer_consumer

.PHONY: all clean run test-small test-large

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# Default run: 3 producers, 2 consumers, buffer size 10
run: all
	./$(TARGET) 3 2 10

# Bonus: small buffer run (for metrics comparison)
test-small: all
	./$(TARGET) 4 4 2

# Bonus: large buffer run (for metrics comparison)
test-large: all
	./$(TARGET) 4 4 32

clean:
	rm -f $(TARGET) results/metrics.txt
