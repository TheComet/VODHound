#!/bin/sh
cd build-debug/bin || echo "Error: Please run this script from the project's root directory as ./scripts/valgrind-gtk4.sh"

echo "Started valgrind..."
valgrind --num-callers=50 \
	--leak-resolution=high \
	--leak-check=full \
	--track-origins=yes \
	--time-stamp=yes \
    --suppressions=/usr/share/gtk-4.0/valgrind/gtk.supp \
	./VODHound-gtk4 2>&1 | tee ../../VODHound-gtk4.grind
cd .. && cd ..

