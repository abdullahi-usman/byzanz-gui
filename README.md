# byzanz-gui

A simple graphical front-end for the byzanz command line utility.

To build

```
git clone https://github.com/dahham/byzanz-gui.git
cd byzanz-gui
meson --prefix=/usr build
ninja -C build
sudo ninja -C build install
```
Alternatively use can use <a href="https://wiki.gnome.org/Apps/Builder">Gnome-Builder IDE</a>.

### Requirements
byzanz
gtk+-3.0 >= 3.22
