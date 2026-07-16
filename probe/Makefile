CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -fPIC

QT5_FLAGS := $(shell pkg-config --cflags Qt5Widgets 2>/dev/null)
QT5_LIBS  := $(shell pkg-config --libs   Qt5Widgets 2>/dev/null)
QT6_FLAGS := $(shell pkg-config --cflags Qt6Widgets 2>/dev/null)
QT6_LIBS  := $(shell pkg-config --libs   Qt6Widgets 2>/dev/null)

.PHONY: all clean qt5 qt6

all: qt5 qt6

qt5: qt5-style-elide-test
qt6: qt-style-elide-test

qt5-style-elide-test: qt5-style-elide-test.cpp
	$(CXX) $(CXXFLAGS) $(QT5_FLAGS) -o $@ $< $(QT5_LIBS)

qt-style-elide-test: qt-style-elide-test.cpp
	$(CXX) $(CXXFLAGS) $(QT6_FLAGS) -o $@ $< $(QT6_LIBS)

clean:
	rm -f qt5-style-elide-test qt-style-elide-test
