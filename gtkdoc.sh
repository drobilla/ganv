#!/bin/sh

DOC_MODULE=ganv

mkdir -p docs
cd docs

export CFLAGS="`pkg-config --cflags ganv-1`"
export LDFLAGS="`pkg-config --libs ganv-1`"

# Sources have changed
gtkdoc-scan --module=$DOC_MODULE --source-dir=../ganv
gtkdoc-scangobj --module=$DOC_MODULE
gtkdoc-mkdb --module=$DOC_MODULE --output-format=xml

# XML files have changed
mkdir -p html
cd html && gtkdoc-mkhtml $DOC_MODULE ../ganv-docs.xml && cd -
gtkdoc-fixxref --module=$DOC_MODULE --module-dir=html

cd -