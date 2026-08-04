CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -fPIC

QT5_FLAGS := $(shell pkg-config --cflags Qt5Widgets 2>/dev/null)
QT5_LIBS  := $(shell pkg-config --libs   Qt5Widgets 2>/dev/null)
QT6_FLAGS := $(shell pkg-config --cflags Qt6Widgets 2>/dev/null)
QT6_LIBS  := $(shell pkg-config --libs   Qt6Widgets 2>/dev/null)

# One source, built twice — the same arrangement Breeze itself uses to produce
# breeze5.so and breeze6.so from a single translation unit.
.PHONY: all clean qt5 qt6

all: qt5 qt6

qt5: itemview-testbed-qt5
qt6: itemview-testbed-qt6

itemview-testbed-qt5: itemview-testbed.cpp
	$(CXX) $(CXXFLAGS) $(QT5_FLAGS) -o $@ $< $(QT5_LIBS)

itemview-testbed-qt6: itemview-testbed.cpp
	$(CXX) $(CXXFLAGS) $(QT6_FLAGS) -o $@ $< $(QT6_LIBS)

clean:
	rm -f itemview-testbed-qt5 itemview-testbed-qt6
