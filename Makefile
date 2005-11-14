CFLAGS = -g -Wall -Wextra -Wno-unused-parameter

MVM_OBJS = mvm.o syms.o symlookup.o
mvm: $(MVM_OBJS)
	$(CC) $(CFLAGS) -o mvm $(MVM_OBJS) -lopus -lm

$(MVM_OBJS): mvm.h syms.h symdesc.h

syms.c syms.h: mksyms symdesc.h
	./mksyms

mksyms: mksyms.c symdesc.h
	$(CC) $(CFLAGS) -o mksyms mksyms.c -lopus -lm

clean:
	rm -f *.o *~ mksyms mvm syms.c syms.h TMP.c TMP.h

tar:
	tar -czf mvm.tar.gz Makefile *.[ch]
