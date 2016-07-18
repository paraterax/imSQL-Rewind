all:
	gcc -g -o rewind_audit src/rewind_audit.c `pkg-config --cflags --libs libmongoc-1.0` -lsqlite3	
	gcc -g -o rewind_slow src/rewind_slow.c `pkg-config --cflags --libs libmongoc-1.0` -lsqlite3	
clean:
	rm -f rewind_audit rewind_slow

