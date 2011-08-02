#!/bin/bash

for i in *.markdown; do
	name="${i%%.*}"
	j="${i#*.}"
	num="${j%%.*}"

	echo "$name($num) $name.$num"
done
