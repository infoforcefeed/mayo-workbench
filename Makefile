CFLAGS=-Werror -Wall -O2 -g3
INCLUDES=-pthread -I./include/
NAME=mayoworkbench
LIBS=-l38moths -loleg-http


all: $(NAME)

clean:
	rm -f *.o
	rm -f $(NAME)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(INCLUDES) -c $<

$(NAME): server.o main.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o $(NAME) $^ -lm $(LIBS)
