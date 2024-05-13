#!/bin/bash
clang -c -I. -I./src/common -I./src/include /tmp/postgresql-12.14/src/backend/access/gist/gistget.c
