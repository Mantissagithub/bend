/****************************************************************************/
/*                           the diy toolsuite                              */
/*                                                                          */
/* Jade Alglave, University College London, UK.                             */
/* Luc Maranget, INRIA Paris-Rocquencourt, France.                          */
/*                                                                          */
/* This C source is a product of litmus7 and includes source that is        */
/* governed by the CeCILL-B license.                                        */
/****************************************************************************/
/* Parameters */
#define SIZE_OF_TEST 100000
#define NUMBER_OF_RUN 10
#define AVAIL 3
#define STRIDE (-1)
#define MAX_LOOP 0
#define N 2
#define AFF_INCR (-1)
/* Includes */
#ifdef __ARM_NEON
#include <arm_neon.h>
#define cast(v)			\
({					\
    typeof(v) __in = (v);		\
    int32x4_t __out;			\
    asm("" : "=w"(__out) : "0"(__in));	\
     __out;				\
})
#endif /* __ARM_NEON */
#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif /* __ARM_FEATURE_SVE */
#ifdef __ARM_FEATURE_SME
#include <arm_sme.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include "utils.h"
#include "outs.h"

/* 128 bit types */
typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

/* params */
typedef struct {
  int verbose;
  int size_of_test,max_run;
  int do_change;
} param_t;

typedef uint32_t ins_t; /* Type of instructions */

/***************************/
/* Get instruction opcodes */
/***************************/

static ins_t getnop(void) {
  ins_t *x1;
  ins_t r;
  asm __volatile__ (
    "adr %[x1],0f\n\t"
    "ldr %w[x2],[%[x1]]\n\t"
    "b 1f\n"
    "0:\n\t"
    "nop\n"
    "1:\n"
    :[x1] "=&r" (x1),[x2] "=&r" (r)
    :
    : "cc", "memory"
);
  return r;
}


static ins_t nop;


/* Full memory barrier */
inline static void mbar(void) {
  asm __volatile__ ("dsb sy" ::: "memory");
}

/* Barriers macros */
inline static void barrier_wait(unsigned int id, unsigned int k, int volatile *b) {
  if ((k % N) == id) {
    *b = 1 ;
  } else {
    while (*b == 0) ;
  }
}

/**********************/
/* Context definition */
/**********************/


typedef struct {
/* Shared variables */
  int *lo;
  int *hi;
  int *get;
/* Final content of observed  registers */
  int *out_0_x0;
  int *out_0_x1;
  int *out_1_x0;
/* Check data */
  pb_t *fst_barrier;
/* Barrier for litmus loop */
  int volatile *barrier;
/* Instance seed */
  st_t seed;
/* Parameters */
  param_t *_p;
} ctx_t;

inline static int final_cond(int _out_0_x0,int _out_0_x1,int _out_1_x0) {
  switch (_out_0_x0) {
  case 1:
    switch (_out_0_x1) {
    case 3:
      switch (_out_1_x0) {
      case 1:
        return 1;
      default:
        return 0;
      }
    default:
      return 0;
    }
  default:
    return 0;
  }
}

inline static int final_ok(int cond) {
  return cond;
}

/**********************/
/* Outcome collection */
/**********************/
#define NOUTS 3
typedef intmax_t outcome_t[NOUTS];

static const int out_0_x0_f = 0 ;
static const int out_0_x1_f = 1 ;
static const int out_1_x0_f = 2 ;


typedef struct hist_t {
  outs_t *outcomes ;
  count_t n_pos,n_neg ;
} hist_t ;

static hist_t *alloc_hist(void) {
  hist_t *p = malloc_check(sizeof(*p)) ;
  p->outcomes = NULL ;
  p->n_pos = p->n_neg = 0 ;
  return p ;
}

static void free_hist(hist_t *h) {
  free_outs(h->outcomes) ;
  free(h) ;
}

static void add_outcome(hist_t *h, count_t v, outcome_t o, int show) {
  h->outcomes = add_outcome_outs(h->outcomes,o,NOUTS,v,show) ;
}

static void merge_hists(hist_t *h0, hist_t *h1) {
  h0->n_pos += h1->n_pos ;
  h0->n_neg += h1->n_neg ;
  h0->outcomes = merge_outs(h0->outcomes,h1->outcomes,NOUTS) ;
}


static void do_dump_outcome(FILE *fhist, intmax_t *o, count_t c, int show) {
  fprintf(fhist,"%-6"PCTR"%c>0:X0=%d; 0:X1=%d; 1:X0=%d;\n",c,show ? '*' : ':',(int)o[out_0_x0_f],(int)o[out_0_x1_f],(int)o[out_1_x0_f]);
}

static void just_dump_outcomes(FILE *fhist, hist_t *h) {
  outcome_t buff ;
  dump_outs(fhist,do_dump_outcome,h->outcomes,buff,NOUTS) ;
}

/**************************************/
/* Prefetch (and check) global values */
/**************************************/

static void check_globals(ctx_t *_a) {
  int *lo = _a->lo;
  int *hi = _a->hi;
  int *get = _a->get;
  for (int _i = _a->_p->size_of_test-1 ; _i >= 0 ; _i--) {
    if (rand_bit(&(_a->seed)) && lo[_i] != 1) fatal("TornPop, check_globals failed");
    if (rand_bit(&(_a->seed)) && hi[_i] != 1) fatal("TornPop, check_globals failed");
    if (rand_bit(&(_a->seed)) && get[_i] != 0) fatal("TornPop, check_globals failed");
  }
  pb_wait(_a->fst_barrier);
}

/***************/
/* Litmus code */
/***************/

typedef struct {
  int th_id; /* I am running on this thread */
  ctx_t *_a;   /* In this context */
} parg_t;

typedef void (*code0_t)(int *get,int *hi,int *lo,int *out_0_x0,int *out_0_x1);

__attribute__((noinline)) static void code0(int *get,int *hi,int *lo,int *out_0_x0,int *out_0_x1) {
  int trashed_x2;
asm __volatile__ (
"\n"
"#START _litmus_P0\n"
"#_litmus_P0_0\n\t"
"ldar %w[x0],[%[x4]]\n"
"#_litmus_P0_1\n\t"
"ldr %w[x1],[%[x5]]\n"
"#_litmus_P0_2\n\t"
"mov %w[x2],#1\n"
"#_litmus_P0_3\n\t"
"str %w[x2],[%[x6]]\n"
"#END _litmus_P0\n"
:[x1] "=&r" (*out_0_x1),[x0] "=&r" (*out_0_x0),[x2] "=&r" (trashed_x2)
:[x4] "r" (hi),[x5] "r" (lo),[x6] "r" (get)
:"cc","memory"
);
}

static void *P0(void *_vb) {
  mbar();
  parg_t *_b = (parg_t *)_vb;
  ctx_t *_a = _b->_a;
  check_globals(_a);
  int _th_id = _b->th_id;
  int volatile *barrier = _a->barrier;
  int _size_of_test = _a->_p->size_of_test;
  for (int _i = _size_of_test-1 ; _i >= 0 ; _i--) {
    barrier_wait(_th_id,_i,&barrier[_i]);
    code0(&_a->get[_i],&_a->hi[_i],&_a->lo[_i],&_a->out_0_x0[_i],&_a->out_0_x1[_i]);
  }
  mbar();
  return NULL;
}

typedef void (*code1_t)(int *get,int *hi,int *lo,int *out_1_x0);

__attribute__((noinline)) static void code1(int *get,int *hi,int *lo,int *out_1_x0) {
  int trashed_x1;
  int trashed_x2;
asm __volatile__ (
"\n"
"#START _litmus_P1\n"
"#_litmus_P1_0\n\t"
"ldr %w[x0],[%[x6]]\n"
"#_litmus_P1_1\n\t"
"cmp %w[x0],#1\n"
"#_litmus_P1_2\n\t"
"b.ne 0f\n"
"#_litmus_P1_3\n\t"
"mov %w[x1],#3\n"
"#_litmus_P1_4\n\t"
"str %w[x1],[%[x5]]\n"
"#_litmus_P1_5\n\t"
"mov %w[x2],#3\n"
"#_litmus_P1_6\n\t"
"stlr %w[x2],[%[x4]]\n"
"#_litmus_P1_7\n"
"0:\n"
"#END _litmus_P1\n"
:[x0] "=&r" (*out_1_x0),[x2] "=&r" (trashed_x2),[x1] "=&r" (trashed_x1)
:[x4] "r" (hi),[x5] "r" (lo),[x6] "r" (get)
:"cc","memory"
);
}

static void *P1(void *_vb) {
  mbar();
  parg_t *_b = (parg_t *)_vb;
  ctx_t *_a = _b->_a;
  check_globals(_a);
  int _th_id = _b->th_id;
  int volatile *barrier = _a->barrier;
  int _size_of_test = _a->_p->size_of_test;
  for (int _i = _size_of_test-1 ; _i >= 0 ; _i--) {
    barrier_wait(_th_id,_i,&barrier[_i]);
    code1(&_a->get[_i],&_a->hi[_i],&_a->lo[_i],&_a->out_1_x0[_i]);
  }
  mbar();
  return NULL;
}

/*******************************************************/
/* Context allocation, freeing and reinitialization    */
/*******************************************************/

static void init_getinstrs(void) {
  nop = getnop();
}

static void init(ctx_t *_a) {
  int size_of_test = _a->_p->size_of_test;

  _a->seed = rand();
  _a->out_0_x0 = malloc_check(size_of_test*sizeof(*(_a->out_0_x0)));
  _a->out_0_x1 = malloc_check(size_of_test*sizeof(*(_a->out_0_x1)));
  _a->out_1_x0 = malloc_check(size_of_test*sizeof(*(_a->out_1_x0)));
  _a->lo = malloc_check(size_of_test*sizeof(*(_a->lo)));
  _a->hi = malloc_check(size_of_test*sizeof(*(_a->hi)));
  _a->get = malloc_check(size_of_test*sizeof(*(_a->get)));
  _a->fst_barrier = pb_create(N);
  _a->barrier = malloc_check(size_of_test*sizeof(*(_a->barrier)));
}

static void finalize(ctx_t *_a) {
  free((void *)_a->lo);
  free((void *)_a->hi);
  free((void *)_a->get);
  free((void *)_a->out_0_x0);
  free((void *)_a->out_0_x1);
  free((void *)_a->out_1_x0);
  pb_free(_a->fst_barrier);
  free((void *)_a->barrier);
}

static void reinit(ctx_t *_a) {
  for (int _i = _a->_p->size_of_test-1 ; _i >= 0 ; _i--) {
    _a->lo[_i] = 1;
    _a->hi[_i] = 1;
    _a->get[_i] = 0;
    _a->out_0_x0[_i] = -239487;
    _a->out_0_x1[_i] = -239487;
    _a->out_1_x0[_i] = -239487;
    _a->barrier[_i] = 0;
  }
}

typedef struct {
  pm_t *p_mutex;
  pb_t *p_barrier;
  param_t *_p;
} zyva_t;

#define NT N

static void *zyva(void *_va) {
  zyva_t *_a = (zyva_t *) _va;
  param_t *_b = _a->_p;
  pb_wait(_a->p_barrier);
  pthread_t thread[NT];
  parg_t parg[N];
  f_t *fun[] = {&P0,&P1};
  hist_t *hist = alloc_hist();
  ctx_t ctx;
  ctx._p = _b;

  init(&ctx);
  for (int _p = N-1 ; _p >= 0 ; _p--) {
    parg[_p].th_id = _p; parg[_p]._a = &ctx;
  }

  for (int n_run = 0 ; n_run < _b->max_run ; n_run++) {
    if (_b->verbose>1) fprintf(stderr,"Run %i of %i\r", n_run, _b->max_run);
    reinit(&ctx);
    if (_b->do_change) perm_funs(&ctx.seed,fun,N);
    for (int _p = NT-1 ; _p >= 0 ; _p--) {
      launch(&thread[_p],fun[_p],&parg[_p]);
    }
    if (_b->do_change) perm_threads(&ctx.seed,thread,NT);
    for (int _p = NT-1 ; _p >= 0 ; _p--) {
      join(&thread[_p]);
    }
    /* Log final states */
    for (int _i = _b->size_of_test-1 ; _i >= 0 ; _i--) {
      int _out_0_x0_i = ctx.out_0_x0[_i];
      int _out_0_x1_i = ctx.out_0_x1[_i];
      int _out_1_x0_i = ctx.out_1_x0[_i];
      outcome_t o;
      int cond;

      cond = final_ok(final_cond(_out_0_x0_i,_out_0_x1_i,_out_1_x0_i));
      o[out_0_x0_f] = _out_0_x0_i;
      o[out_0_x1_f] = _out_0_x1_i;
      o[out_1_x0_f] = _out_1_x0_i;
      add_outcome(hist,1,o,cond);
      if (cond) { hist->n_pos++; } else { hist->n_neg++; }
    }
  }

  finalize(&ctx);
  return hist;
}

#define ENOUGH 10

static void postlude(FILE *out,cmd_t *cmd,hist_t *hist,count_t p_true,count_t p_false,tsc_t total) {
  fprintf(out,"Test TornPop Allowed\n");
  fprintf(out,"Histogram (%d states)\n",finals_outs(hist->outcomes));
  just_dump_outcomes(out,hist);
  int cond = p_true > 0;
  fprintf(out,"%s\n",cond?"Ok":"No");
  fprintf(out,"\nWitnesses\n");
  fprintf(out,"Positive: %" PCTR ", Negative: %" PCTR "\n",p_true,p_false);
  fprintf(out,"Condition %s is %svalidated\n","exists (0:X0=1 /\\ 0:X1=3 /\\ 1:X0=1)",cond ? "" : "NOT ");
  fprintf(out,"Hash=6ac07ac0fd24de4e51c5cbe2d7ea9a8e\n");
  count_t cond_true = p_true;
  count_t cond_false = p_false;
  fprintf(out,"Observation TornPop %s %" PCTR " %" PCTR "\n",!cond_true ? "Never" : !cond_false ? "Always" : "Sometimes",cond_true,cond_false);
  fprintf(out,"Time TornPop %.2f\n",total / 1000000.0);
  fflush(out);
}

static void run(cmd_t *cmd,cpus_t *def_all_cpus,FILE *out) {
  tsc_t start = timeofday();
  init_getinstrs();
  param_t prm ;
/* Set some parameters */
  prm.verbose = cmd->verbose;
  prm.size_of_test = cmd->size_of_test;
  prm.max_run = cmd->max_run;
  prm.do_change = 1;
  if (cmd->fix) prm.do_change = 0;
/* Computes number of test concurrent instances */
  int n_avail = cmd->avail;
  int n_exe;
  if (cmd->n_exe > 0) {
    n_exe = cmd->n_exe;
  } else {
    n_exe = n_avail < N ? 1 : n_avail / N;
  }
/* Show parameters to user */
  if (prm.verbose) {
    log_error( "TornPop: n=%i, r=%i, s=%i",n_exe,prm.max_run,prm.size_of_test);
    log_error("\n");
  }
  hist_t *hist = NULL;
  int n_th = n_exe-1;
  pthread_t th[n_th];
  zyva_t zarg[n_exe];
  pm_t *p_mutex = pm_create();
  pb_t *p_barrier = pb_create(n_exe);
  for (int k=0 ; k < n_exe ; k++) {
    zyva_t *p = &zarg[k];
    p->_p = &prm;
    p->p_mutex = p_mutex; p->p_barrier = p_barrier;
    if (k < n_th) {
      launch(&th[k],zyva,p);
    } else {
      hist = (hist_t *)zyva(p);
    }
  }

  count_t n_outs = prm.size_of_test; n_outs *= prm.max_run;
  for (int k=0 ; k < n_th ; k++) {
    hist_t *hk = (hist_t *)join(&th[k]);
    if (sum_outs(hk->outcomes) != n_outs || hk->n_pos + hk->n_neg != n_outs) {
      fatal("TornPop, sum_hist");
    }
    merge_hists(hist,hk);
    free_hist(hk);
  }
  tsc_t total = timeofday() - start;
  pm_free(p_mutex);
  pb_free(p_barrier);

  n_outs *= n_exe ;
  if (sum_outs(hist->outcomes) != n_outs || hist->n_pos + hist->n_neg != n_outs) {
    fatal("TornPop, sum_hist") ;
  }
  count_t p_true = hist->n_pos, p_false = hist->n_neg;
  postlude(out,cmd,hist,p_true,p_false,total);
  free_hist(hist);
}


int main(int argc, char **argv) {
  cpus_t *def_all_cpus = NULL;
  cmd_t def = { 0, NUMBER_OF_RUN, SIZE_OF_TEST, STRIDE, AVAIL, 0, 0, aff_none, 0, 0, AFF_INCR, def_all_cpus, NULL, -1, MAX_LOOP, NULL, NULL, -1, -1, -1, 0, 0};
  cmd_t cmd = def;
  parse_cmd(argc,argv,&def,&cmd);
  run(&cmd,def_all_cpus,stdout);
  return EXIT_SUCCESS;
}
