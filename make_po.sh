#! /bin/bash
cd po/dhlrc
ls ../../src/*.ui ../../src/config/*.kcfg | sort | xargs extractrc > rc.cpp
xgettext -k_ -kN_ \
-ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ki18nd:2 \
-kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
-kkli18n:1 -kkli18nc:1c,2 -kkli18np:1,2 -kkli18ncp:1c,2,3 \
--default-domain=dhlrc --package-name=dhlrc \
--output=dhlrc.pot ../../src/*.cpp ../../build/src/settings.cpp rc.cpp
echo "Extract the po source file"

#lupdate6 ../../src/qt/*.ui ../../src/qt/*/*.ui -ts dhlrc.ts
#lconvert6 -i dhlrc.ts -of po -o dhlrc_qt.pot
#echo "Extract the qt po file"
#msgcat dhlrc_qt.pot dhlrc.pot -o dhlrc.pot