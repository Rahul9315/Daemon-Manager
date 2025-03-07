CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = daemon_manager

all: $(TARGET)

$(TARGET): daemon_manager.c
	$(CC) $(CFLAGS) -o $(TARGET) daemon_manager.c

run:
	@echo "Starting daemon..." >> daemon_log.txt
	./$(TARGET) &

stop:
	@echo "Stopping daemon..." >> daemon_log.txt
	pkill $(TARGET)

restart: stop
	@sleep 1
	@echo "Restarting daemon..." >> daemon_log.txt
	@make run

clean:
	rm -f $(TARGET)
