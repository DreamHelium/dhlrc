#! /bin/bash

xgettext -k_ -kN_ --default-domain=dhlrc --package-name=dhlrc \
--output=po/dhlrc/dhlrc.pot src/*.c src/qt/*.cpp src/gtk/*.c src/cli/*.c src/*.cpp
echo "Extract the po source file"

lupdate6 src/qt/*.ui -ts po/dhlrc/dhlrc.ts
lconvert6 -i po/dhlrc/dhlrc.ts -of po -o po/dhlrc/dhlrc_qt.pot
echo "Extract the qt po file"
msgcat po/dhlrc/dhlrc_qt.pot po/dhlrc/dhlrc.pot -o po/dhlrc/dhlrc.pot