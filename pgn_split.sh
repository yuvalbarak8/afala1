#!/bin/bash

# check if the numbers of arguments is valid
if test "$#" -ne 2;
then
  echo "Usage: $0 <source_pgn_file> <destination directory>"
  exit 1
fi

# check if the input file exist
if [ ! -e "$1" ];
then
  echo "Error: File '$1' does not exist."
  exit 1
fi

# check if the directory exist, and crate if not
if [ ! -d "$2" ];
then
  mkdir -p "$2"
  echo "Created directory '$2'."
fi
