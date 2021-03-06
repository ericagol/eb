#include <stdlib.h>
#include <float.h>
#include <math.h>

#include "eb.h"

double eb_phisec (double esinw, double ecosw) {
  double esinwsq, ecoswsq, esq, num, phi;

  esinwsq = esinw*esinw;
  ecoswsq = ecosw*ecosw;
  esq = esinwsq + ecoswsq;

  /* Mean anomaly offset from inferior conjunction to superior
     conjunction (= approximate phase of secondary eclipse). */
  num = 2 * ecosw * sqrt(1.0 - esq);
  phi = atan2(-num, esq + ecoswsq - 1.0) + num / (1.0 - esinwsq);
  phi = fmod(phi, TWOPI);
  if(phi < 0)
    phi += TWOPI;

  return(phi / TWOPI);
}

#define ITER_MAX 20

void eb_phicont (double esinw, double ecosw, double cosi,
                 double d, double *phi) {
  double esq, ecc, omesq, romesq, roe, sinw, cosw;
  double csqi, omcsqi, dsq, num;
  double dsec, dcec, dc;
  double cvw, svw, cvwsq, svwsq, rr, drr, f, df, delta;
  double sv, cv, dse, dce, dd;
  double dphi;
  int i, n, iter;

  struct {
    double s, c;
  } sign[] = {
      {  1.0,  1.0 },  /* primary 1st */
      {  1.0, -1.0 },  /* primary last */
      { -1.0, -1.0 },  /* secondary 1st */
      { -1.0,  1.0 }   /* secondary last */
  };

  esq = esinw*esinw + ecosw*ecosw;
  ecc = sqrt(esq);
  omesq = 1.0 - esq;
  romesq = 1.0 / omesq;
  roe = sqrt(omesq);

  if(ecc > 0) {
    cosw = ecosw / ecc;
    sinw = esinw / ecc;
  }
  else {
    cosw = 0.0;
    sinw = 1.0;
  }

  csqi = cosi*cosi;
  omcsqi = 1.0 - csqi;
  dsq = d*d;

  /* d* sin, cos eccentric anomaly at inferior conjunction */
  dsec = roe * cosw;
  dcec = ecc + sinw;
  dc = 1.0 + esinw;

  n = sizeof(sign) / sizeof(sign[0]);

  for(i = 0; i < n; i++) {
    /* We need to solve d^2 = r^2(cos^2(v+w) + sin^2(v+w) cos^2(i))
       this is a quartic in cos(v+w).  The solution is fairly messy
       so here we just do it numerically. */

    /* Radius vector at conjunction and test for an eclipse */
    rr = (1.0 + sign[i].s * esinw) * romesq;  /* 1/r */
    num = dsq*rr*rr - csqi;  /* d^2/r^2 - cos^2 (i) */
    
    /* Is there an eclipse? */
    if(num > 0) {
      /* Initial guess for cos(v+w) under the approximation that
         r changes slowly during the event, so we can use the value
         exactly at conjunction. */
      cvw = sign[i].c * sqrt(num / omcsqi);

      /* Newton-Raphson */
      for(iter = 0; iter < ITER_MAX; iter++) {
        /* sin(v+w) */
        cvwsq = cvw*cvw;
        svwsq = 1.0-cvwsq;

        if(svwsq < 0) {
          /* No solution, just return conjunction phases */
          sv = sign[i].s * cosw;
          cv = sign[i].s * sinw;
          break;
        }

        svw = sign[i].s * sqrt(svwsq);
        
        /* sin(v) and cos(v) */
        sv = svw * cosw - cvw * sinw;
        cv = cvw * cosw + svw * sinw;
        
        /* 1/r and d(1/r)/d(cvw) */
        rr  = (1.0 + ecc*cv) * romesq;
        drr = ecc*sv*romesq / svw;

        /* f = cos^2(v+w) (1 - cos^2(i)) + cos^2(i) - d^2/r^2 */
        f  = cvwsq*omcsqi + csqi - dsq * rr*rr;
        df = 2 * (cvw * omcsqi - dsq * rr*drr);
        
        delta = f / df;
        
        if(fabs(delta) < DBL_EPSILON) {
          /* I think that's enough... */
          break;
        }
        
        cvw -= delta;
      }
    }      
    else {
      /* No eclipse, just return (equal) phases of conjunction */
      sv = sign[i].s * cosw;
      cv = sign[i].s * sinw;
    }

    /* d*sin(E) and d*cos(E) */
    dse = sv * roe;
    dce = ecc + cv;
    dd = 1.0 + ecc*cv;

    /* M-MC = E-EC - e (sin E - sin EC)
       
       E-EC = atan2(dc * sin (E-EC),
                    dc * cos (E-EC))
       
       and use sin E and dc*sin(EC) from above for rest. */
    
    dphi = atan2(dse * dcec - dce * dsec,
                 dce * dcec + dse * dsec)
         - ecc * (dse*dc - dsec*dd)/(dd*dc);
    dphi = fmod(dphi, TWOPI);
    if(dphi < 0)
      dphi += TWOPI;

    phi[i] = dphi / TWOPI;
  }
}

void eb_getvder (double *v, double gamma, double ktot, double *vder) {
  double esq, roe, sini, qpo, tmp, omega;
  double phi[4], dpp, dps;

  vder[EB_PAR_I] = acos(v[EB_PAR_COSI]) * 180.0/M_PI;
  vder[EB_PAR_R1A] = v[EB_PAR_RASUM] / (1.0 + v[EB_PAR_RR]);
  vder[EB_PAR_R2A] = v[EB_PAR_RR] * vder[EB_PAR_R1A];

  esq = v[EB_PAR_ECOSW]*v[EB_PAR_ECOSW] + v[EB_PAR_ESINW]*v[EB_PAR_ESINW];

  vder[EB_PAR_E] = sqrt(esq);
  vder[EB_PAR_OMEGA] = atan2(v[EB_PAR_ESINW], v[EB_PAR_ECOSW]) * 180.0/M_PI;
  
  roe = sqrt(1.0-esq);
  sini = sqrt(1.0-v[EB_PAR_COSI]*v[EB_PAR_COSI]);
  qpo = 1.0+v[EB_PAR_Q];

  /* Orbital angular frequency in system barycenter.  The
   * factor (1.0+gamma/c) accounts for the Doppler shift
   * due to the systemic motion relative to the solar system
   * barycenter, which is the rest frame the period was
   * calculated in.
   */
  omega = 2.0*M_PI*(1.0 + gamma*1000/EB_LIGHT) / (v[EB_PAR_P]*86400);

  tmp = ktot*1000 * roe;

  vder[EB_PAR_A] = tmp / (EB_RSUN*omega*sini);
  vder[EB_PAR_MTOT] = tmp*tmp*tmp / (EB_GMSUN*omega*sini);
  vder[EB_PAR_M1] = vder[EB_PAR_MTOT] / qpo;
  vder[EB_PAR_M2] = v[EB_PAR_Q] * vder[EB_PAR_M1];
  vder[EB_PAR_RTOT] = v[EB_PAR_RASUM] * vder[EB_PAR_A];
  vder[EB_PAR_R1] = vder[EB_PAR_R1A] * vder[EB_PAR_A];
  vder[EB_PAR_R2] = vder[EB_PAR_R2A] * vder[EB_PAR_A];

  /* factor of 100 converts to cgs */
  vder[EB_PAR_LOGG1] = log10(100 * EB_GMSUN * vder[EB_PAR_M1] /
                        (EB_RSUN*EB_RSUN*vder[EB_PAR_R1]*vder[EB_PAR_R1]));
  vder[EB_PAR_LOGG2] = log10(100 * EB_GMSUN * vder[EB_PAR_M2] /
                        (EB_RSUN*EB_RSUN*vder[EB_PAR_R2]*vder[EB_PAR_R2]));

  /* I think this is only for circular orbits */
  vder[EB_PAR_VSYNC1] = 1.0e-3 * omega * vder[EB_PAR_R1]*EB_RSUN;
  vder[EB_PAR_VSYNC2] = 1.0e-3 * omega * vder[EB_PAR_R2]*EB_RSUN;

  /* Eq. 6.1 of Zahn 1977 for stars with convective envelopes except in Gyr */
  vder[EB_PAR_TSYNC] = 1.0e-5 * v[EB_PAR_P]*v[EB_PAR_P]*v[EB_PAR_P]*v[EB_PAR_P]
                     * qpo*qpo / (4*v[EB_PAR_Q]*v[EB_PAR_Q]);

  /* Eq. 6.2 of Zahn 1977 for stars with convective envelopes except in Gyr */
  vder[EB_PAR_TCIRC] = 1.0e-3 * pow((1.0+v[EB_PAR_Q]) / 2, 5.0/3.0)
                     * pow(v[EB_PAR_P], 16.0/3.0) / v[EB_PAR_Q];

  vder[EB_PAR_TSEC] = v[EB_PAR_T0]
                    + v[EB_PAR_P]*eb_phisec(v[EB_PAR_ESINW], v[EB_PAR_ECOSW]);
  
  eb_phicont(v[EB_PAR_ESINW], v[EB_PAR_ECOSW],
             v[EB_PAR_COSI], v[EB_PAR_RASUM], phi);

  dpp = phi[1] - phi[0];
  if(dpp < 0)
    dpp += 1.0;

  dps = phi[3] - phi[2];
  if(dps < 0)
    dps += 1.0;

  vder[EB_PAR_DURPRI] = dpp * v[EB_PAR_P] * 24;
  vder[EB_PAR_DURSEC] = dps * v[EB_PAR_P] * 24;
}
  
