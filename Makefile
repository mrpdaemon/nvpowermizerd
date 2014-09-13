nvpowermizerd: nvpowermizerd.c
	gcc -o nvpowermizerd -L/usr/X11R6/lib/ -lX11 -lXext -lXss nvpowermizerd.c

clean:
	rm -f nvpowermizerd
