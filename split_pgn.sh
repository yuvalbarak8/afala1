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

# check if the directory exist, and create if not
if [ ! -d "$2" ];
then
  mkdir -p "$2"
  echo "Created directory '$2'."
fi

# Search for lines starting with "[event" and print line numbers
lines=$(grep -n "^\[Event " "$1" | cut -d ":" -f 1)

# Convert the string of line numbers into an array
IFS=$'\n' read -r -d '' -a line_array <<< "$lines"

# Get the filename without extension
filename=$(basename "$1" .pgn)

# Loop through line_array to create files
for ((i = 0; i < ${#line_array[@]}; i++))
do
    # Determine the start and end lines for each file
    start_line=${line_array[i]}
    end_line=${line_array[i+1]}
    if [ -z "$end_line" ]; then
        # Last iteration, use the end of file
        end_line='$'
    else
        # Subtract 1 from end line to exclude the next [Event line
        end_line=$((end_line - 1))
    fi

    # Create the file with lines from start_line to end_line
    sed -n "${start_line},${end_line}p" "$1" > "$2/${filename}_$((i+1)).pgn"
    echo "Saved game to $2/${filename}_$((i+1)).pgn"
done
echo "All games have been split and saved to '$2'."
