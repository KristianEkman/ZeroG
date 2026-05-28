#!/bin/bash

set -e

pgnout_args=()
savefen_args=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--pgnout)
			if [[ $# -lt 2 ]]; then
				echo "usage: $0 [--pgnout <file>] [--savefen <file>]" >&2
				exit 1
			fi

			pgnout_args=(-pgnout "$2" fi)
			shift 2
			;;
		--savefen)
			if [[ $# -lt 2 ]]; then
				echo "usage: $0 [--pgnout <file>] [--savefen <file>]" >&2
				exit 1
			fi

			savefen_args=(option.SaveQuietPositionsFile="$2")
			shift 2
			;;
		*)
			echo "usage: $0 [--pgnout <file>] [--savefen <file>]" >&2
			exit 1
			;;
	esac
done

../cutechess/cutechess/build/cutechess-cli \
 -engine cmd=./builds/ChessAI2027_prev proto=uci name=OLD \
 -engine cmd=./builds/ChessAI2027 proto=uci name=NEW "${savefen_args[@]}" \
 -each tc=3+0.01 \
 -games 300 \
 -openings file=./grand_master_openings.epd format=epd order=random \
 -repeat \
 -concurrency 4 \
 "${pgnout_args[@]}"