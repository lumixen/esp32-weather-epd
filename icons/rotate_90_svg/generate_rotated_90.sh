#!/bin/bash
# Generate rotated SVG assets.
# Copyright (C) 2022  Luke Marzen
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


INPUT=$1

mkdir -p rotated

for DEG in {0..359..90}; 
do
OUTPUT="./rotated/`basename $INPUT .svg`_`expr \( $DEG \) % 360`deg.svg"

cp $INPUT $OUTPUT

sed -i -e "s/rotate(0/rotate($DEG/g" $OUTPUT
echo $DEG
done
