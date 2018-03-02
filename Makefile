CC=g++
CFLAGS=
LIBS=

watermeter: watermeterMQTT.o mqtt.o
	$(CC) -o watermeterMQTT watermeterMQTT.o mqtt.o $(CFLAGS) $(LIBS)

clean:
	rm watermeterMQTT.o
	rm watermeterMQTT
	rm mqtt.o
	