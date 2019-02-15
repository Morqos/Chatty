# Compile client
gcc -I ../include client.c connections.c -o client

# Compile server
gcc -I ../include chatty.c config.c icl_hash.c threadpool.c connections.c -o chatty -lpthread

# Create Useful directories
# mkdir ../../../../../../../tmp/chatty_socket
# mkdir ../../../../../../../tmp/chatty
# rm -f ../../../../../../../tmp/chatty/testFile.txt