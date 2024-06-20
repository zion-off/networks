all: client server

client: udp_client.c
	gcc udp_client.c -o client
	
server: udp_server.c
	gcc udp_server.c -o server

clean:
	rm -f client
	rm -f server
