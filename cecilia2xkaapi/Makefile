test:
	./c2x -v
	./c2x -s test_splitter -i toto.h dest_test src_test

install:
	mkdir -p ~/bin
	cp -i c2x $(HOME)/bin/

clean:
	rm -fr *.c *.h dest_test/*
