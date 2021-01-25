all: client server
#		./server 5000 &
#		./client localhost 5000 '/home/arthur/Learning/7sem/net/coursework/example/client'

client: old_client.c
		gcc client.c -lm -lpthread -oclient

server: old_server.c
		gcc server.c -lm -lpthread -oserver

clean:
		rm client server
