#!/bin/sh
GCC=gcc
GCCOPTS="-Wall -std=gnu99 -O2"
LINKOPTS=""
/bin/rm -f *.exe *.s
$GCC $GCCOPTS -O2 -c outs.c
$GCC $GCCOPTS -O2 -c utils.c
$GCC $GCCOPTS -O2 -c litmus_rand.c
$GCC $GCCOPTS $LINKOPTS -o TornPop.exe outs.o utils.o litmus_rand.o TornPop.c
$GCC $GCCOPTS -S TornPop.c && awk -f show.awk TornPop.s > TornPop.t && /bin/rm TornPop.s
$GCC $GCCOPTS $LINKOPTS -o TornPopFixed.exe outs.o utils.o litmus_rand.o TornPopFixed.c
$GCC $GCCOPTS -S TornPopFixed.c && awk -f show.awk TornPopFixed.s > TornPopFixed.t && /bin/rm TornPopFixed.s
$GCC $GCCOPTS $LINKOPTS -o MP.exe outs.o utils.o litmus_rand.o MP.c
$GCC $GCCOPTS -S MP.c && awk -f show.awk MP.s > MP.t && /bin/rm MP.s
$GCC $GCCOPTS $LINKOPTS -o LB.exe outs.o utils.o litmus_rand.o LB.c
$GCC $GCCOPTS -S LB.c && awk -f show.awk LB.s > LB.t && /bin/rm LB.s
