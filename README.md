# Building GTK4 for Windows with MSVC

Within a VS Native Tools Shell:
```bat
# Clone GTK
git clone https://gitlab.gnome.org/GNOME/gtk.git
cd gtk
git checkout 4.13.2  # Or a more recent version

# Setup meson
python -m venv venv
venv\Scripts\activate.bat
pip install meson

# Configure build -- gstreamer doesn't appear to work out of the box on windows
# and VODHound has its own video player built from scratch, so it is disabled here
meson setup build --prefix C:\gnome -Doptimization=3 -Dbuild-demos=false -Dbuild-examples=false -Dbuild-testsuite=false -Dbuild-tests=false -Ddebug=false -Dmedia-gstreamer=disabled

# Build and install
meson compile -C build
meson install -C build
```

