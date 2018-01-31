#include "qm.h"
#include "eq.h"
#include "matrix.h"

void scf(int Na, int Nb, double * Ca, double * Cb,
    double * Va, double * Vb,
    double * Da, double * Db, double * Dmp,
    int maxit, double dDmax, int * alo, int * alv,
    double * H, double * Hmp, double * mmmm, double * pmmm,
    basis * bo, basis * bv, mol * m, qmdata * qmd, FILE * fo){

  int Mo = bo->M;
  int Mv = bv->M;

  double * Fa    = malloc(sizeof(double)*symsize(Mo));
  double * Fb    = malloc(sizeof(double)*symsize(Mo));
  double * oldD  = malloc(sizeof(double)*symsize(Mo));
  double * Fw    = malloc(sizeof(double)*symsize(Mo));
  double * Fmpa  = malloc(sizeof(double)*Mo*Mv);
  double * Fmpb  = malloc(sizeof(double)*Mo*Mv);
  double * Xa    = malloc(sizeof(double)*Mo*Mv);
  double * Xb    = malloc(sizeof(double)*Mo*Mv);
  double * FaXa  = malloc(sizeof(double)*Mo*Mo);
  double * FbXb  = malloc(sizeof(double)*Mo*Mo);
  double * sa    = malloc(sizeof(double)*Mo);
  double * sb    = malloc(sizeof(double)*Mo);
  double * F2a   = malloc(sizeof(double)*symsize(Mo));
  double * F2b   = malloc(sizeof(double)*symsize(Mo));
  double * FA    = malloc(sizeof(double)*symsize(Mo));
  double * FB    = malloc(sizeof(double)*symsize(Mo));
  double * dEdFa = malloc(sizeof(double)*Mo*Mv);
  double * dEdFb = malloc(sizeof(double)*Mo*Mv);

  double E0 = E0_eq2(m, qmd);
  double E1 = 0.0;
  double E2 = 0.0;
  double oldE;
  vecset(symsize(Mo), oldD, 0.0);
  int k = 0;

  while(k++ < maxit){
    oldE = E1+E2;
    D_eq9 (Na, Mo, Ca, Da);
    D_eq9 (Nb, Mo, Cb, Db);
    F_eq4 (Da, Db, H, Fa, Fb, alo, mmmm, bo, m, qmd);
    F2_8_7_14_15_6(Da, Db, Hmp, Fmpa, Fmpb, Xa, Xb, FaXa, FbXb, sa, sb, F2a, F2b, alo, alv, pmmm, bo, bv, m, qmd);
    Heff(Da, Db, Xa, Xb, FaXa, FbXb, sa, sb, Fa, Fb, F2a, F2b, Fmpa, Fmpb, FA, FB, dEdFa, dEdFb, alo, alv, pmmm, bo, bv, m, qmd);
    mx_id(Mo, Ca);
    veccp(symsize(Mo), Fw, FA);
    jacobi(Fw, Ca, Va, Mo, 1e-15, 20, NULL);
    eigensort(Mo, Va, Ca);
    mx_id(Mo, Cb);
    veccp(symsize(Mo), Fw, FB);
    jacobi(Fw, Cb, Vb, Mo, 1e-15, 20, NULL);
    eigensort(Mo, Vb, Cb);
    // TODO
    E1 = E1_eq3(Mo, H, Da, Db, Fa, Fb);
    E2 = E2_eq5(Mo, Da, Db, F2a, F2b);

    double dD = 0.0;
    for(int i=0; i<symsize(Mo); i++){
      double ab = Da[i]+Db[i];
      double d  = oldD[i] - ab;
      dD += d*d;
      oldD[i] = ab;
    }

    double dE = E1+E2-oldE;
    fprintf(fo, " it %3d     E = % 17.10lf    dE = % 17.10lf    dD = % 5.2e\n", k, E0+E1+E2, k==1?0.0:dE, dD);
    if(dD < dDmax){
      fprintf(fo, "converged\n");
      break;
    }
  }

  fprintf(fo, "\n");
  fprintf(fo, " (E0   = %20.10lf)\n", E0);
  fprintf(fo, " (E0+1 = %20.10lf)\n", E0+E1);
  fprintf(fo, " (E2   = %20.10lf)\n", E2);
  fprintf(fo, "  E    = %20.10lf\n",  E0+E1+E2);

  vecsum(Mo*Mv, Dmp, dEdFa, dEdFb);

  free(Fa);
  free(Fb);
  free(oldD);
  free(Fw);
  free(Fmpa);
  free(Fmpb);
  free(Xa);
  free(Xb);
  free(FaXa);
  free(FbXb);
  free(sa);
  free(sb);
  free(F2a);
  free(F2b);
  free(FA);
  free(FB);
  free(dEdFa);
  free(dEdFb);

  return;
}

