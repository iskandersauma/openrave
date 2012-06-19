#!/bin/bash
# argument $1 is language: en, ja
# build the python documentation and openrave homepage
if [ "$1" == "ja" ]; then
    export LANG=ja_JP.UTF-8
else
    export LANG=en_US.UTF-8
fi
rm -rf _templates/examples.html _templates/databases.html _templates/database_generator_template.rst source/$1/openravepy build/$1/main
python build_interfaces.py --outdir=source
if [ "$?" -ne 0 ]; then echo "build_interfaces.py failed"; exit 1; fi 
mkdir -p source/$1/openravepy
ln -s -f `openrave-config --python-dir`/openravepy/_openravepy_ openravepy
export PYTHONPATH=`pwd`:$PYTHONPATH
python sphinx-autopackage-script/generate_modules.py --dest-dir=source/$1/openravepy --suffix=rst --maxdepth=3 --no-toc --sep-files `pwd`/openravepy
rm openravepy
if [ "$?" -ne 0 ]; then echo "build_interfaces.py failed"; exit 1; fi 
python build_ikdatabase.py --lang=$1 --ikfaststats=ikfaststats.pp
if [ "$?" -ne 0 ]; then echo "build_ikdatabase.py failed"; fi 
ln -s -f `openrave-config --python-dir`/openravepy/_openravepy_ openravepy
sphinx-build -b html -c . source build/$1/main
rm openravepy
if [ "$?" -ne 0 ]; then echo "sphinx-build failed"; exit 1; fi 
