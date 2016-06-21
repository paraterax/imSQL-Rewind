all:
	gcc -g -o rewind src/rewind.c `pkg-config --cflags --libs libmongoc-1.0` -lsqlite3	
clean:
	rm -f rewind

