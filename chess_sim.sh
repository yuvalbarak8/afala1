#!/bin/bash
#Find the line number where the second part starts
line_num=$(grep -n "^[0-9]\." <<< "$(cat "$1")" | head -n 1 | cut -d: -f1)

# Split into two parts
part1=$(head -n +""$((line_num - 1))"" <<< "$(cat "$1")")
echo "$part1"
part2=$(tail -n +"$line_num" <<< "$(cat "$1")")
moves_list=$(python3 parse_moves.py "$part2")
moves_number=$(echo $moves_list | wc -w)

declare -A chessboard
init()
{
# Populate the chessboard
for ((move = 0; move <=moves_number; move++)); do
  for row in {1..8}; do
    for col in {a..h}; do
      chessboard[$move$row$col]="."
    done
  done
done

  # White pawns
  for col in {a..h}; do
    chessboard[0"2"$col]="P"
  done

  # Black pawns
  for col in {a..h}; do
    chessboard[0"7"$col]="P"
  done

  # Other White pieces
  chessboard[0"1a"]="R"
  chessboard[0"1b"]="N"
  chessboard[0"1c"]="B"
  chessboard[0"1d"]="Q"
  chessboard[0"1e"]="K"
  chessboard[0"1f"]="B"
  chessboard[0"1g"]="N"
  chessboard[0"1h"]="R"

  # Other Black pieces
  chessboard[0"8a"]="R"
  chessboard[0"8b"]="N"
  chessboard[0"8c"]="B"
  chessboard[0"8d"]="Q"
  chessboard[0"8e"]="K"
  chessboard[0"8f"]="B"
  chessboard[0"8g"]="N"
  chessboard[0"8h"]="R"

 # Set up pieces for each move
local current_move=1
for move in $moves_list; do
    prev_move=$((current_move - 1))
    for row in {1..8}; do
        for col in {a..h}; do
            chessboard[$current_move$row$col]=${chessboard[$prev_move$row$col]}
        done
    done
    # Update the board for the current move
    from_col="${move:0:1}"
    from_row="${move:1:1}"
    to_col="${move:2:1}"
    to_row="${move:3:1}"
    piece="${chessboard[$prev_move$from_row$from_col]}"
    chessboard[$current_move$to_row$to_col]="$piece"
    chessboard[$current_move$from_row$from_col]="."
    ((current_move++))
done

}
# Function to display the chessboard
display_board() {
  local move="$1"
  echo "Move $move/$moves_number"
  echo "   a  b  c  d  e  f  g  h"
  for ((row = 8; row >= 1; row--)); do
    echo -n "$row "
    for col in {a..h}; do
      echo -n " ${chessboard[$move$row$col]} "
    done
    echo "$row "
  done
  echo "   a  b  c  d  e  f  g  h"
  echo "Press 'd' to move forward, 'a' to move back, 'w' to go to the start, 's' to go to the end, 'q' to quit:"

  while true; do
    read -rsn1 key
    case "$key" in
      d) ((move < $moves_number)) && ((move++)); display_board $move; break ;;
      a) ((move > 0)) && ((move--)); display_board $move; break ;;
      w) move=0; display_board $move; break ;;
      s) move=$moves_number; display_board $move; break ;;
      q) echo "Exiting." 
         exit;;
      *) echo "Invalid key pressed: $key" ;;
    esac
  done
}

# Display the initial chessboard
init
display_board 0
