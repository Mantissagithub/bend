#!/bin/sh

date
LITMUSOPTS="${@:-$LITMUSOPTS}"
SLEEP=0
if [ ! -f TornPop.no ]; then
cat <<'EOF'
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Results for TornPop.litmus %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
AArch64 TornPop
"BendRT ring: consumer pops slot 0 (task 1), producer refills it with task 3. Instructions as clang -O3 emits them."

{
 [lo]=1;
 [hi]=1;
 [get]=0;
 0:X4=hi; 0:X5=lo; 0:X6=get;
 1:X4=hi; 1:X5=lo; 1:X6=get;
}
 P0           | P1           ;
 LDAR W0,[X4] | LDR W0,[X6]  ;
 LDR W1,[X5]  | CMP W0,#1    ;
 MOV W2,#1    | B.NE LC00    ;
 STR W2,[X6]  | MOV W1,#3    ;
              | STR W1,[X5]  ;
              | MOV W2,#3    ;
              | STLR W2,[X4] ;
              | LC00:        ;

exists (0:X0=1 /\ 0:X1=3 /\ 1:X0=1)
Generated assembler
EOF
cat TornPop.t
./TornPop.exe -q $LITMUSOPTS
fi
sleep $SLEEP

if [ ! -f TornPopFixed.no ]; then
cat <<'EOF'
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Results for TornPopFixed.litmus %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
AArch64 TornPopFixed
"BendRT ring: consumer pops slot 0 (task 1), producer refills it with task 3. Instructions as clang -O3 emits them."

{
 [lo]=1;
 [hi]=1;
 [get]=0;
 0:X4=hi; 0:X5=lo; 0:X6=get;
 1:X4=hi; 1:X5=lo; 1:X6=get;
}
 P0           | P1           ;
 LDAR W0,[X4] | LDR W0,[X6]  ;
 LDR W1,[X5]  | CMP W0,#1    ;
 MOV W2,#1    | B.NE LC00    ;
 STLR W2,[X6] | MOV W1,#3    ;
              | STR W1,[X5]  ;
              | MOV W2,#3    ;
              | STLR W2,[X4] ;
              | LC00:        ;

exists (0:X0=1 /\ 0:X1=3 /\ 1:X0=1)
Generated assembler
EOF
cat TornPopFixed.t
./TornPopFixed.exe -q $LITMUSOPTS
fi
sleep $SLEEP

if [ ! -f MP.no ]; then
cat <<'EOF'
%%%%%%%%%%%%%%%%%%%%%%%%%
% Results for MP.litmus %
%%%%%%%%%%%%%%%%%%%%%%%%%
AArch64 MP
"control: message passing with no ordering, known weak on ARM"

{
 0:X1=x; 0:X3=y;
 1:X1=y; 1:X3=x;
}
 P0          | P1          ;
 MOV W0,#1   | LDR W0,[X1] ;
 STR W0,[X1] | LDR W2,[X3] ;
 MOV W2,#1   |             ;
 STR W2,[X3] |             ;

exists (1:X0=1 /\ 1:X2=0)
Generated assembler
EOF
cat MP.t
./MP.exe -q $LITMUSOPTS
fi
sleep $SLEEP

if [ ! -f LB.no ]; then
cat <<'EOF'
%%%%%%%%%%%%%%%%%%%%%%%%%
% Results for LB.litmus %
%%%%%%%%%%%%%%%%%%%%%%%%%
AArch64 LB
"control: plain load buffering, the reordering TornPop needs"

{
 0:X1=x; 0:X3=y;
 1:X1=y; 1:X3=x;
}
 P0          | P1          ;
 LDR W0,[X1] | LDR W0,[X1] ;
 MOV W2,#1   | MOV W2,#1   ;
 STR W2,[X3] | STR W2,[X3] ;

exists (0:X0=1 /\ 1:X0=1)
Generated assembler
EOF
cat LB.t
./LB.exe -q $LITMUSOPTS
fi
sleep $SLEEP

cat <<'EOF'
Revision exported, version 7.58
Command line: litmus7 -os mac -mode std -a 3 -o l7 TornPop.litmus TornPopFixed.litmus MP.litmus LB.litmus
Parameters
#define SIZE_OF_TEST 100000
#define NUMBER_OF_RUN 10
#define AVAIL 3
#define STRIDE (-1)
#define MAX_LOOP 0
/* gcc options: -Wall -std=gnu99 -O2 */
/* barrier: user */
/* launch: changing */
/* affinity: none */
/* memory: direct */
/* safer: write */
/* preload: random */
/* speedcheck: no */
/* alloc: dynamic */
/* proc used: 3 */
EOF
sed '2q;d' comp.sh
echo "LITMUSOPTS=$LITMUSOPTS"
date
