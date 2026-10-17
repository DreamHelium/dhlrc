#! /bin/bash
cd po/dhlrc
ls ../../src/*.ui | sort | xargs extractrc > rc.cpp
xgettext -k_ -kN_ \
-ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ki18nd:2 \
-kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
-kkli18n:1 -kkli18nc:1c,2 -kkli18np:1,2 -kkli18ncp:1c,2,3 \
--default-domain=dhlrc --package-name=dhlrc \
--output=dhlrc.pot ../../src/*.cpp rc.cpp ../../*/src/*.rs ../../src/*.h ../../src/dhsettingsdialog/*.cpp ../../src/dhsettingsdialog/*.h
echo "Extract the po source file"

# The playful catalogue is generated from the plain one, so it never drifts
# far behind. Python is only needed here; without it the checked-in file is
# used as it is.
cd ../..
if command -v python3 > /dev/null; then
  python3 po/make_cute.py
else
  echo "python3 not found: the playful catalogue was not regenerated"
fi
exit 0
