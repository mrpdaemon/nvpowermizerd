nvpowermizerd: nvpowermizerd.c
	gcc ${CFLAGS} -o nvpowermizerd nvpowermizerd.c -L/usr/X11R6/lib/ -lX11 -lXext -lXss

clean:
	rm -f nvpowermizerd
