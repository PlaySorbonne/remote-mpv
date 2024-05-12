CC := gcc
CFLAGS := -Wall -Werror

SRCDIR := src
SRCS := $(notdir $(wildcard $(SRCDIR)/*.c))
OBJS := $(SRCS:.c=.o)

BUILDDIR := build
OBJSDIR := $(BUILDDIR)/objects
OUTDIR := $(BUILDDIR)/out

EXEC := remote-mpv
PORT := 8080
SOCKETPATH := /tmp/mpvsocket


all: build run

build: INITBUILDDIR $(OBJS)
	gcc $(addprefix $(OBJSDIR)/, $(OBJS)) -o $(OUTDIR)/$(EXEC)

run:
	$(OUTDIR)/./$(EXEC) -p $(PORT) -s $(SOCKETPATH)

clean:
	rm -f $(addprefix $(OBJSDIR)/, $(OBJS))
	rmdir $(OBJSDIR)
	rm -f $(OUTDIR)/$(EXEC)
	rmdir $(OUTDIR)
	rmdir $(BUILDDIR)

INITBUILDDIR:
	mkdir -p $(OBJSDIR)
	mkdir -p $(OUTDIR)

# Define a pattern rule that compiles every .c file into a .o file
%.o: $(SRCDIR)/%.c
		$(CC) -c $(CFLAGS) $< -o $(OBJSDIR)/$@
