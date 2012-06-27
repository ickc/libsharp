/*
 *  This file is part of libsharp.
 *
 *  libsharp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libsharp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libsharp; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  libsharp is being developed at the Max-Planck-Institut fuer Astrophysik
 *  and financially supported by the Deutsches Zentrum fuer Luft- und Raumfahrt
 *  (DLR).
 */

/*! \file sharp_bench.c
    Copyright (C) 2012 Max-Planck-Society
    \author Martin Reinecke
*/

#include <stdio.h>
#include <string.h>
#ifdef USE_MPI
#include "mpi.h"
#endif
#include "sharp.h"
#include "sharp_geomhelpers.h"
#include "sharp_almhelpers.h"
#include "c_utils.h"
#include "sharp_core.h"

typedef complex double dcmplx;

static void bench_sht (int spin, int nv, sharp_jobtype type,
  int ntrans, double *time, unsigned long long *opcnt)
  {
  int lmax=2047;
  int mmax=128;
  int nrings=512;
  int ppring=1024;
  ptrdiff_t npix=(ptrdiff_t)nrings*ppring;
  sharp_geom_info *tinfo;
  sharp_make_gauss_geom_info (nrings, ppring, 1, ppring, &tinfo);

  ptrdiff_t nalms = ((mmax+1)*(mmax+2))/2 + (mmax+1)*(lmax-mmax);
  int ncomp = ntrans*((spin==0) ? 1 : 2);

  double **map;
  ALLOC2D(map,double,ncomp,npix);
  SET_ARRAY(map[0],0,npix*ncomp,0.);

  sharp_alm_info *alms;
  sharp_make_triangular_alm_info(lmax,mmax,1,&alms);

  dcmplx **alm;
  ALLOC2D(alm,dcmplx,ncomp,nalms);
  SET_ARRAY(alm[0],0,nalms*ncomp,0.);

  int nruns=0;
  sharp_job job;
  sharpd_build_job(&job,type,spin,0,&alm[0],&map[0],tinfo,alms,ntrans);
  job.nv=nv;
  *time=1e30;
  *opcnt=1000000000000000;
  do
    {
    sharpd_build_job(&job,type,spin,0,&alm[0],&map[0],tinfo,alms,ntrans);
    job.nv=nv;
    sharp_execute_job(&job);

    if (job.opcnt<*opcnt) *opcnt=job.opcnt;
    if (job.time<*time) *time=job.time;
    }
  while (++nruns < 4);

  DEALLOC2D(map);
  DEALLOC2D(alm);

  sharp_destroy_alm_info(alms);
  sharp_destroy_geom_info(tinfo);
  }

int main(void)
  {
#ifdef USE_MPI
  MPI_Init(NULL,NULL);
#endif
  module_startup_c("sharp_bench",1,1,"",1);

  printf("Benchmarking SHTs.\n\n");
  FILE *fp=fopen("oracle.inc","w");
  UTIL_ASSERT(fp, "failed to open oracle file for writing");
  fprintf(fp,"static const int maxtr = 6;\n");
  fprintf(fp,"static const int nv_opt[6][2][3] = {\n");

  for (int ntr=1; ntr<=6; ++ntr)
    {
    fprintf(fp,"{");
    for (int spin=0; spin<=2; spin+=2)
      {
      fprintf(fp,"{");
      for (sharp_jobtype type=MAP2ALM; type<=ALM2MAP; ++type)
        {
        int nvbest=-1, nvoracle=sharp_nv_oracle(type,spin,ntr);
        unsigned long long opmin=1000000000000000, op;
        double tmin=1e30;
        double *time=RALLOC(double,sharp_get_nv_max()+1);
        for (int nv=1; nv<=sharp_get_nv_max(); ++nv)
          {
          bench_sht (spin,nv,type,ntr,&time[nv],&op);
          if (op<opmin) opmin=op;
          if (time[nv]<tmin)
            { tmin=time[nv]; nvbest=nv; }
          }
        printf("nt: %d  %s  spin: %d   nv: %d   time: %6.3f   perf: %6.3f"
          "   dev[%d]: %6.2f%%\n",ntr,(type==ALM2MAP)?"alm2map":"map2alm",
          spin,nvbest,tmin,opmin/tmin*1e-9,nvoracle,
          (time[nvoracle]-tmin)/tmin*100.);
        DEALLOC(time);
        fprintf(fp,"%d",nvbest);
        fprintf(fp,(type==MAP2ALM)?",":",-1");
        }
      fprintf(fp,(spin==0)?"},":"}");
      printf("\n");
      }
    fprintf(fp,(ntr<6)?"},\n":"}\n");
    }
  fprintf(fp,"};\n");
  fclose(fp);
#ifdef USE_MPI
  MPI_Finalize();
#endif
  return 0;
  }