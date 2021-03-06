#include "qm.h"
#include "matrix.h"
#include "eq.h"
#include "2el.h"
#include "tools.h"
#include "mytime.h"
#include "gradient.h"
#include <ctype.h>

#define print_def   1
#define dDmax_def   1e-8
#define maxit_def  32
#define memit_def   8
#define diis_def    1
#define rhf_def     1
#define task_def    ENERGY

static const s16 tasks_s[] = {
  [NIL]    = "NIL",
  [ENERGY] = "ENERGY",
  [GRAD]   = "GRAD",
  [TDHF]   = "TDHF",
  [CIS]    = "CIS",
  [POLAR]  = "POLAR"
};

static inline void str_toupper(char * s){
  while(*s){
    *s = toupper(*s);
    s++;
  }
  return;
}

int main(int argc, char * argv[]){

  FILE * fm;
  FILE * fp;
  if (!((argc >= 1) && (fp = fopen(argv[1], "r"))))  GOTOHELL;
  if (!((argc >= 2) && (fm = fopen(argv[2], "r"))))  GOTOHELL;

  double dDmax = dDmax_def;
  int    maxit = maxit_def;
  int    print = print_def;
  int    memit = memit_def;
  int    diis  = diis_def;
  int    rhf   = rhf_def;
  char   vi[256]  = {0};
  char   vo[256]  = {0};
  int    ffield   =  0;
  double field[3] = {0};
  char   task_s[256] = {0};
  FILE * fo = stdout;
  for(int i=3; i<argc; i++){
    if( sscanf (argv[i], "conv:%lf",    &dDmax         ) ) { continue; }
    if( sscanf (argv[i], "it:%d,%d",    &maxit, &memit ) ) { continue; }
    if( sscanf (argv[i], "print:%d",    &print         ) ) { continue; }
    if( sscanf (argv[i], "read:%255s",  &vi            ) ) { continue; }
    if( sscanf (argv[i], "write:%255s", &vo            ) ) { continue; }
    if( sscanf (argv[i], "diis:%d",     &diis          ) ) { continue; }
    if( sscanf (argv[i], "restrict:%d", &rhf           ) ) { continue; }
    if( sscanf (argv[i], "task:%255s",  &task_s        ) ) { continue; }
    if( (ffield = sscanf (argv[i], "field: %lf,%lf,%lf", field, field+1, field+2))) { continue; }
    if(! (fo = fopen(argv[i], "w"))){
      fo = stdout;
    }
  }
  if(ffield!=3){
    ffield = 0;
  }
  fprintf(fo, "\n"VERSION"\n");
  fprintf(fo, "conv:%e\n", dDmax);
  fprintf(fo, "it:%d", maxit);
  if(diis){
    fprintf(fo, ",%d", memit);
  }
  fprintf(fo,"\n");
  if(ffield){
    fprintf(fo, "field:%e,%e,%e\n", field[0], field[1], field[2]);
  }

  str_toupper(task_s);
  task_t task = task_def;
  for(task_t i=0; i<sizeof(tasks_s)/sizeof(tasks_s[0]); i++){
    if(!strcmp(task_s, tasks_s[i])){
      task = i;
      break;
    }
  }

  qmdata * qmd = qmdata_read(fp);
  fclose(fp);
#if 0
  qmdata_print(fo, qmd);
#endif
  mol * m = mol_read(fm);
  fclose(fm);
  if(!m){
    GOTOHELL;
  }

  int N = 0;
  for(int i=0; i<m->n; i++){
    N += nel(m->q[i], qmd);
  }
  N -= m->z;
  int Nu = m->mult-1;
  if( (N-Nu)%2 ) {
    fprintf(stderr, "\tN = %d, mult = %d !\n", N, m->mult);
    free(m);
    free(qmd);
    fclose(fo);
    return 1;
  }

  int Nb = (N-Nu)/2;
  int Na = N-Nb;
  int Mo = norb(m, qmd->Lo);
  int Mv = norb(m, qmd->Lv);
  if(Na>Mo){
    fprintf(stderr, "\tNa = %d, Mo = %d !\n", Na, Mo);
    free(m);
    free(qmd);
    fclose(fo);
    return 1;
  }

  fprintf(fo, "\n");
  fprintf(fo, "  N   = %d,", N);
  fprintf(fo, "  Na  = %d,", Na);
  fprintf(fo, "  Nb  = %d\n", Nb);
  fprintf(fo, "  Mo  = %d,", Mo);
  fprintf(fo, "  Mv  = %d\n", Mv);
  fprintf(fo, "\n");

  basis * bo  = basis_fill(Mo, m, qmd->Lo);
  basis * bv  = basis_fill(Mv, m, qmd->Lv);
  int   * alo = basis_al(m, qmd->Lo);
  int   * alv = basis_al(m, qmd->Lv);

  double time_sec = myutime();

  // integrals -----------------------------------------------------------------

  double * f    = malloc(sizeof(double)*symsize(Mo));
  double * fmp  = malloc(sizeof(double)*Mo*Mv);
  double * H    = malloc(sizeof(double)*symsize(Mo));
  double * Hmp  = malloc(sizeof(double)*Mo*Mv);
  double * rij  = malloc(sizeof(double)*symsize(m->n));
  axis   * xyz  = malloc(sizeof(axis )*(m->n)*(m->n));
  distang(rij, xyz, m);
  double * mmmm = mmmm0_fill(alo, rij, xyz, bo, m, qmd);
  double * pmmm = pmmm_fill (alo, alv, xyz, bo, bv, m, qmd);
  f_eq25_mm(f,   xyz, alo,      bo,     m, qmd);
  f_eq25_mp(fmp, xyz, alo, alv, bo, bv, m, qmd);
  H_eq22_mm(f,   H,   alo,      mmmm, bo,     m, qmd);
  H_eq22_mp(fmp, Hmp, alo, alv, pmmm, bo, bv, m, qmd);
  mmmm6_add(alo, mmmm, rij, bo, m, qmd);
#if 0
  mmmm_check(mmmm, bo, m, qmd);
  pmmm_check(pmmm, bo, bv, m, qmd);
#endif
  free(xyz);
  free(rij);

  double E0 = E0_eq2(m, qmd);

  if(ffield){
    H_ext(field, H, alo, bo, m, qmd);
    Hmp_ext(field, Hmp, alo, alv, bo, bv, m, qmd);
    E0 += E0_ext(field, m, qmd);
  }

  // ---------------------------------------------------------------------------

  int restricted = (Na==Nb ? 1 : 0) && rhf;

  if(restricted){
    double * C   = malloc(sizeof(double)*Mo*Mo);
    double * V   = malloc(sizeof(double)*Mo);
    double * D   = malloc(sizeof(double)*symsize(Mo));
    double * Dmp = malloc(sizeof(double)*Mo*Mv);

    if(vi[0] && pvec_read(V, V, C, C, vi, bo)){
      fprintf(fo, " read coefficients from '%s'\n\n", vi);
    }
    else{
      double * Fw = malloc(sizeof(double)*symsize(Mo));
      mx_id(Mo, C);
      veccp(symsize(Mo), Fw, f);
      jacobi(Fw, C, V, Mo, 1e-15, 20, NULL);
      eigensort(Mo, V, C);
      free(Fw);
    }

    if(diis){
      scf_diis_r(N, E0, C, V, D, Dmp, maxit, memit, dDmax, alo, alv, H, Hmp, mmmm, pmmm, bo, bv, m, qmd, fo);
    }
    else{
      scf_r   (N, E0, C, V, D, Dmp, maxit, dDmax, alo, alv, H, Hmp, mmmm, pmmm, bo, bv, m, qmd, fo);
    }
    double dip[3] = {0.0};
    dipole(D, D, Dmp, dip, alo, alv, bo, bv, m, qmd);
    fprintf(fo, " dipole: %+10lf %+10lf %+10lf\n", dip[0], dip[1], dip[2]);

    if(print > 1){
      fprintf(fo, "\n  scf vectors:\n");
      mo_table(Na, V, C, bo, fo);
    }

    if(print > 2){
      population(D, D, alo, m, qmd, fo);
    }

    time_sec = myutime()-time_sec;
    fprintf(fo, "\nTIME: %.2lf sec  (%.2lf min)\n\n", time_sec, time_sec/60.0);

    if(vo[0] && pvec_write(V, V, C, C, vo, bo)){
      fprintf(fo, " wrote coefficients to '%s'\n\n", vo);
    }
    fflush(fo);

    if(task==GRAD){
      double time_sec = myutime();
      double * g = malloc(m->n*3*sizeof(double));
      gradient_r(D, Hmp, pmmm, g, alo, alv, bo, bv, m, qmd);
      if(ffield){
        E0_ext_grad(field, g, D, D, alo, m, qmd);
      }
      g_print(m->n, g, "", fo);
      free(g);
      time_sec = myutime()-time_sec;
      fprintf(fo, "TIME: %.2lf sec  (%.2lf min)\n\n", time_sec, time_sec/60.0);
    }

    free(C);
    free(V);
    free(D);
    free(Dmp);
    free(f);
    free(fmp);
    free(H);
    free(Hmp);
  }

  else{
    double * Ca   = malloc(sizeof(double)*Mo*Mo);
    double * Cb   = malloc(sizeof(double)*Mo*Mo);
    double * Va   = malloc(sizeof(double)*Mo);
    double * Vb   = malloc(sizeof(double)*Mo);
    double * Da   = malloc(sizeof(double)*symsize(Mo));
    double * Db   = malloc(sizeof(double)*symsize(Mo));
    double * Dmp  = malloc(sizeof(double)*Mo*Mv);

    if(vi[0] && pvec_read(Va, Vb, Ca, Cb, vi, bo)){
      fprintf(fo, " read coefficients from '%s'\n\n", vi);
    }
    else{
      double * Fw = malloc(sizeof(double)*symsize(Mo));
      mx_id(Mo, Ca);
      veccp(symsize(Mo), Fw, f);
      jacobi(Fw, Ca, Va, Mo, 1e-15, 20, NULL);
      eigensort(Mo, Va, Ca);
      veccp(Mo*Mo, Cb, Ca);
      free(Fw);
    }

    if(diis){
      scf_diis(Na, Nb, E0, Ca, Cb, Va, Vb, Da, Db, Dmp, maxit, memit, dDmax, alo, alv, H, Hmp, mmmm, pmmm, bo, bv, m, qmd, fo);
    }
    else{
      scf     (Na, Nb, E0, Ca, Cb, Va, Vb, Da, Db, Dmp, maxit, dDmax, alo, alv, H, Hmp, mmmm, pmmm, bo, bv, m, qmd, fo);
    }
    double dip[3] = {0.0};
    dipole(Da, Db, Dmp, dip, alo, alv, bo, bv, m, qmd);
    fprintf(fo, " dipole: %+10lf %+10lf %+10lf\n", dip[0], dip[1], dip[2]);

    double * Sab = Sab_fill(Mo, Ca, Cb);
    s2uhf(Mo, Na, Nb, Sab, fo);

    if(print > 1){
      fprintf(fo, "  alpha:\n");
      mo_table(Na, Va, Ca, bo, fo);
      fprintf(fo, "  beta:\n");
      mo_table(Nb, Vb, Cb, bo, fo);
    }

    if(print > 2){
      population(Da, Db, alo, m, qmd, fo);
    }

    time_sec = myutime()-time_sec;
    fprintf(fo, "\nTIME: %.2lf sec  (%.2lf min)\n\n", time_sec, time_sec/60.0);

    if(vo[0] && pvec_write(Va, Vb, Ca, Cb, vo, bo)){
      fprintf(fo, " wrote coefficients to '%s'\n\n", vo);
    }
    fflush(fo);

#if 0
    Deff_test(Na, Nb, Ca, Cb, Hmp, Dmp, alo, alv, pmmm, bo, bv, m, qmd);
#endif
#if 0
    Heff_test(Na, Nb, Ca, Cb, H, Hmp, alo, alv, mmmm, pmmm, bo, bv, m, qmd);
#endif

    if(task==GRAD){
      double time_sec = myutime();
      double * g = malloc(m->n*3*sizeof(double));
      gradient(Da, Db, Hmp, pmmm, g, alo, alv, bo, bv, m, qmd);
      if(ffield){
        E0_ext_grad(field, g, Da, Db, alo, m, qmd);
      }
      g_print(m->n, g, "", fo);
      free(g);
      time_sec = myutime()-time_sec;
      fprintf(fo, "TIME: %.2lf sec  (%.2lf min)\n\n", time_sec, time_sec/60.0);
    }
#if 0
    gradient_test(Na, Nb, Da, Db, Hmp, pmmm, alo, alv, bo, bv, m, qmd); // without field
#endif
#if 0
    A_grad_test();
#endif
    free(Sab);
    free(Ca);
    free(Cb);
    free(Va);
    free(Vb);
    free(Da);
    free(Db);
    free(Dmp);
    free(f);
    free(fmp);
    free(H);
    free(Hmp);
  }

  free(alo);
  free(alv);
  free(bo);
  free(bv);
  free(m);
  free(mmmm);
  free(pmmm);
  free(qmd);
  fclose(fo);
  return 0;
}

