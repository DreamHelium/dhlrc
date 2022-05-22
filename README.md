# Litematica Reader in C language

## Introduction

It's a simple litematica reader written in C in order to get material list with id instead of block translation names, and then use programs to process material list.
Now it could read blocks in litematica file and exclude some "blocks" (e.g air), change some block names (e.g "water" to "water_bucket").

It uses [libnbt](https://github.com/djytw/libnbt) as the library to read litematica file.
