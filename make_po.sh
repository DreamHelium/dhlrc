#! /bin/bash

xgettext -k_ -kN_ --default-domain=dhlrc --package-name=dhlrc \
--output=po/dhlrc/dhlrc.pot src/*.c src/qt/*.cpp src/gtk/*.c src/cli/*.c
echo "Extract the po source file"

lupdate-qt6 src/qt/*.ui -ts po/dhlrc/dhlrc.ts
lconvert-qt6 -i po/dhlrc/dhlrc.ts -of po -o po/dhlrc/dhlrc_qt.pot
echo "Extract the qt po file"
msgcat po/dhlrc/dhlrc_qt.pot po/dhlrc/dhlrc.pot -o po/dhlrc/dhlrc.pot