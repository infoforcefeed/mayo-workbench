CFLAGS=-Werror -Wall -O2 -g3
INCLUDES=-pthread -I./include/
NAME=mayobench
COMMON_OBJ=http.o db.o utils.o vector.o logging.o


all: $(NAME)

clean:
	rm -f *.o
	rm -f $(NAME)

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(LIB_INCLUDES) $(INCLUDES) -c $<

$(NAME): $(COMMON_OBJ) grengine.o greshunkel.o server.o stack.o main.o
	$(CC) $(CLAGS) $(LIB_INCLUDES) $(INCLUDES) -o $(NAME) $^ -lm $(LIBS)
