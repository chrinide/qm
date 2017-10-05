#include "eq.h"
#include "tools.h"
#include "vec3.h"
#include "matrix.h"

int qlll_mm(int qu, int lu, int lv, int l, double * q, qmdata * qmd){
  if(l==0){
    *q = 1.0;
    return 1;
  }
  if(lu>lv){
    int t;
    SWAP(lu,lv,t);
  }
  int bra = qmd->q_list[qu-1].qa;
  int ket = qmd->q_list[qu  ].qa;
  for(int i=bra; i<ket; i++){
    if ( (lu == qmd->qa[i].lu) &&
         (lv == qmd->qa[i].lv) &&
         (l  == qmd->qa[i].l)){
      *q = qmd->qa[i].qa;
      return 1;
    }
  }
  return 0;
}

int qlll_pm(int qu, int la, int lv, int l, double * q, qmdata * qmd){
  int bra = qmd->q_list[qu-1].q1a;
  int ket = qmd->q_list[qu  ].q1a;
  for(int i=bra; i<ket; i++){
    if ( (la == qmd->q1a[i].lu) &&
         (lv == qmd->q1a[i].lv) &&
         (l  == qmd->q1a[i].l)){
      *q = qmd->q1a[i].qa;
      return 1;
    }
  }
  return 0;
}

double fundconst(int l, int l1, int m){
  const double q0l[] = {
    [0] = 1.0,
    [1] = SQRT3,
    [2] = 6.70820393249936908920
  };
  const double q11[] = {
    [0] = 6.0,
    [1] = 3.0
  };
  const double q12[] = {
    [0] = 34.85685011586675196661,
    [1] = 20.12461179749810726768
  };
  const double q22[] = {
    [0] = 270.0,
    [1] = 180.0,
    [2] =  45.0
  };
  if(l>l1){
    int t;
    SWAP(l,l1,t);
  }
  if(l==0){
    return q0l[l1];
  }
  unsigned int am = abs(m);
  if( l==1 && l1==1 ){
    return q11[am];
  }
  else if( l==2 && l1==2 ){
    return q22[am];
  }
  else if( l==1 && l1==2){
    return q12[am];
  }
  GOTOHELL;
}

void dipole(double * Da, double * Db, double * Dmp,
    double dip[3], int * alo, int * alv,
    basis * bo, basis * bv, mol * m, qmdata * qmd){

  r3set(dip, 0.0);
  for(int k=0; k<m->n; k++){
    int Q = m->q[k];
    double z = nel(Q, qmd);
    for(int v=alo[k]; v<alo[k+1]; v++){
      int lv = bo->l[v];
      int mv = bo->m[v];
      // min-min dip.:
      for(int u=alo[k]; u<alo[k+1]; u++){
        int lu = bo->l[u];
        int mu = bo->m[u];
        double q;
        if(! qlll_mm(Q, lu, lv, 1, &q, qmd) ) continue;
        double b[3];
        b[0] = B(lu,lv,1,mu,mv, 1);
        b[1] = B(lu,lv,1,mu,mv,-1);
        b[2] = B(lu,lv,1,mu,mv, 0);
        int uv = MPOSIF(u,v);
        r3adds(dip, b, -(Da[uv]+Db[uv]) * q * SQRT3);
      }
      // min-pol dip.:
      for(int a=alv[k]; a<alv[k+1]; a++){
        int la = bv->l[a];
        int ma = bv->m[a];
        double q;
        if(! qlll_pm(Q, la, lv, 1, &q, qmd) ) continue;
        double b[3];
        b[0] = B(la,lv,1,ma,mv, 1);
        b[1] = B(la,lv,1,ma,mv,-1);
        b[2] = B(la,lv,1,ma,mv, 0);
        r3adds(dip, b, -Dmp[a*bo->M+v] * q * SQRT3);
      }
      // mono:
      int i = mpos(v,v);
      z -= Da[i] + Db[i];
    }
    r3adds(dip, m->r+k*3, z);
  }
  return;
}

