CC = g++
CFLAGS = -Wall
LFLAGS = -ljsmn -lopencv_core -lopencv_core -lopencv_videoio -lopencv_imgcodecs

OBJS = app.o
MAIN = app

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS) $(LFLAGS)
	
app.o:
	$(CC) $(CFLAGS) -c app.cpp
	
clean:
	rm $(OBJS) MAIN