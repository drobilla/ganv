#!/bin/sh

DOC_MODULE=ganv

pwd=`pwd`
echo "gtkdoc.sh: Entering directory \`$pwd/docs'"

mkdir -p docs
cd docs

export CFLAGS="`pkg-config --cflags ganv-1`"
export LDFLAGS="`pkg-config --libs ganv-1`"

# Sources have changed

gtkdoc-scan --rebuild-sections --rebuild-types --ignore-headers=types.h --module=$DOC_MODULE --source-dir=../ganv
gtkdoc-scangobj --module=$DOC_MODULE
gtkdoc-mkdb --module=$DOC_MODULE --output-format=xml --source-dir=../ganv

# XML files have changed
mkdir -p html
cd html && gtkdoc-mkhtml $DOC_MODULE ../ganv-docs.xml && cd -
gtkdoc-fixxref --module=$DOC_MODULE --module-dir=html

echo "gtkdoc.sh: Leaving directory \`$pwd/docs'"
cd -
