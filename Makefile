CC=g++
CFLAGS=
LIBS=

watermeter: watermeter.o mqtt.o
	$(CC) -o watermeter watermeter.o mqtt.o $(CFLAGS) $(LIBS)

clean:
	rm watermeter.o
	rm watermeter