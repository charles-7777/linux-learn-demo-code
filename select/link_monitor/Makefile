CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = link_monitor
SRCS = link_monitor.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

run: $(TARGET)
	sudo ./$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: clean $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

help:
	@echo "可用目标:"
	@echo "  all      - 编译程序"
	@echo "  clean    - 清理"
	@echo "  run      - 运行程序 (需要root)"
	@echo "  debug    - 编译调试版本"
	@echo "  install  - 安装到系统"
	@echo "  uninstall- 卸载"
	@echo "  help     - 显示帮助"

.PHONY: all clean run debug install uninstall help