#!/bin/bash
arecord -f cd --device=hw:2,1 |  ./awesome $*
