CC := gcc
CFLAGS := -Wall -Wextra -g

TARGET = nytor

SRCDIR = src
OBJDIR = build

SRCS = main.c buffer.c file.c input.c render.c vector.c cursor.c scroll.c
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)