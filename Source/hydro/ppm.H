
#include <cmath>
#include "reconstruction.H"

#ifndef _ppm_H_
#define _ppm_H_

using namespace amrex;

///
/// Compute the coefficients of a parabolic reconstruction of the data in a zone.
/// This uses the standard PPM limiters described in Colella & Woodward (1984)
///
/// @param s      Real[5] the state to be reconstructed in zones i-2, i-1, i, i+1, i+2
/// @param flatn  The flattening coefficient
/// @param sm     The value of the parabola on the left edge of the zone
/// @param sp     The value of the parabola on the right edge of the zone
///
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void
ppm_reconstruct(const Real* s,
                const Real flatn, Real& sm, Real& sp) {


  // Compute van Leer slopes

  Real dsl = 2.0_rt * (s[im1] - s[im2]);
  Real dsr = 2.0_rt * (s[i0] - s[im1]);

  Real dsvl_l = 0.0_rt;
  if (dsl*dsr > 0.0_rt) {
    Real dsc = 0.5_rt * (s[i0] - s[im2]);
    dsvl_l = std::copysign(1.0_rt, dsc) * amrex::min(std::abs(dsc),amrex::min(std::abs(dsl),std::abs(dsr)));
  }

  dsl = 2.0_rt * (s[i0] - s[im1]);
  dsr = 2.0_rt * (s[ip1] - s[i0]);

  Real dsvl_r = 0.0_rt;
  if (dsl*dsr > 0.0_rt) {
    Real dsc = 0.5_rt * (s[ip1] - s[im1]);
    dsvl_r = std::copysign(1.0_rt, dsc) * amrex::min(std::abs(dsc),amrex::min(std::abs(dsl),std::abs(dsr)));
  }

  // Interpolate s to edges

  sm = 0.5_rt * (s[i0] + s[im1]) - (1.0_rt/6.0_rt) * (dsvl_r - dsvl_l);

  // Make sure sedge lies in between adjacent cell-centered values

  sm = amrex::max(sm, amrex::min(s[i0], s[im1]));
  sm = amrex::min(sm, amrex::max(s[i0], s[im1]));


  // Compute van Leer slopes

  dsl = 2.0_rt * (s[i0] - s[im1]);
  dsr = 2.0_rt * (s[ip1] - s[i0]);

  dsvl_l = 0.0_rt;
  if (dsl*dsr > 0.0_rt) {
    Real dsc = 0.5_rt * (s[ip1] - s[im1]);
    dsvl_l = std::copysign(1.0_rt, dsc) * amrex::min(std::abs(dsc), amrex::min(std::abs(dsl),std::abs(dsr)));
  }

  dsl = 2.0_rt * (s[ip1] - s[i0]);
  dsr = 2.0_rt * (s[ip2] - s[ip1]);

  dsvl_r = 0.0_rt;
  if (dsl*dsr > 0.0_rt) {
    Real dsc = 0.5_rt * (s[ip2] - s[i0]);
    dsvl_r = std::copysign(1.0_rt, dsc) * amrex::min(std::abs(dsc), amrex::min(std::abs(dsl),std::abs(dsr)));
  }

  // Interpolate s to edges

  sp = 0.5_rt * (s[ip1] + s[i0]) - (1.0_rt/6.0_rt) * (dsvl_r - dsvl_l);

  // Make sure sedge lies in between adjacent cell-centered values

  sp = amrex::max(sp, amrex::min(s[ip1], s[i0]));
  sp = amrex::min(sp, amrex::max(s[ip1], s[i0]));


  // Flatten the parabola

  sm = flatn * sm + (1.0_rt - flatn) * s[i0];
  sp = flatn * sp + (1.0_rt - flatn) * s[i0];

  // Modify using quadratic limiters -- note this version of the limiting comes
  // from Colella and Sekora (2008), not the original PPM paper.

  if ((sp - s[i0]) * (s[i0] - sm) <= 0.0_rt) {
    sp = s[i0];
    sm = s[i0];

  } else if (std::abs(sp - s[i0]) >= 2.0_rt * std::abs(sm - s[i0])) {
    sp = 3.0_rt * s[i0] - 2.0_rt * sm;

  } else if (std::abs(sm - s[i0]) >= 2.0_rt * std::abs(sp - s[i0])) {
    sm = 3.0_rt * s[i0] - 2.0_rt * sp;
  }

}


///
/// Integrate under the parabola using from the left and right edges
/// with the wave speeds u-c, u, u+c
///
/// @param sm    The parabola value on the left edge of the zone
/// @param sp    The parabola value on the right edge of the zone
/// @param sc    The state value at the zone center
/// @param u     The fluid velocity
/// @param c     The sound speed
/// @param dtdx  dt/dx (timestep / cell width)
/// @param Ip    Real[3], the integrals under the parabola over
///              the 3 waves from the right zone edge
/// @param Im    Real[3], the integrals under the parabola over
///              the 3 waves from the left zone edge
///
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void
ppm_int_profile(const Real sm, const Real sp, const Real sc,
                const Real u, const Real c, const Real dtdx,
                Real* Ip, Real* Im) {

  // Integrate the parabolic profile to the edge of the cell.

  // compute x-component of Ip and Im
  Real s6 = 6.0_rt * sc - 3.0_rt * (sm + sp);

  // Ip/m is the integral under the parabola for the extent
  // that a wave can travel over a timestep
  //
  // Ip integrates to the right edge of a cell
  // Im integrates to the left edge of a cell

  // u-c wave
  Real speed = u - c;
  Real sigma = std::abs(speed) * dtdx;

  // if speed == ZERO, then either branch is the same
  if (speed <= 0.0_rt) {
    Ip[0] = sp;
    Im[0] = sm + 0.5_rt * sigma * (sp - sm + (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
  } else {
    Ip[0] = sp - 0.5_rt * sigma * (sp - sm - (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
    Im[0] = sm;
  }

  // u wave
  speed = u;
  sigma = std::abs(speed) * dtdx;

  if (speed <= 0.0_rt) {
    Ip[1] = sp;
    Im[1] = sm + 0.5_rt * sigma * (sp - sm + (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
  } else {
    Ip[1] = sp - 0.5_rt * sigma * (sp - sm - (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
    Im[1] = sm;
  }

  // u+c wave
  speed = u + c;
  sigma = std::abs(speed) * dtdx;

  if (speed <= 0.0_rt) {
    Ip[2] = sp;
    Im[2] = sm + 0.5_rt * sigma * (sp - sm + (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
  } else {
    Ip[2] = sp - 0.5_rt * sigma * (sp - sm - (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
    Im[2] = sm;
  }

}

///
/// Integrate under the parabola from the left and right edges for a
/// single wave speed
///
/// @param sm    The parabola value on the left edge of the zone
/// @param sp    The parabola value on the right edge of the zone
/// @param sc    The state value at the zone center
/// @param lam   The wave speed
/// @param dtdx  dt/dx (timestep / cell width)
/// @param Ip    integral under the parabola from the right zone edge
/// @param Im    integral under the parabola from the left zone edge
///
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void
ppm_int_profile_single(const Real sm, const Real sp, const Real sc,
                       const Real lam, const Real dtdx,
                       Real& Ip, Real& Im) {

  // Integrate the parabolic profile to the edge of the cell.
  // This is the MHD version.  We come in already with the eigenvalues.

  // compute x-component of Ip and Im
  Real s6 = 6.0_rt * sc - 3.0_rt * (sm + sp);

  // Ip/m is the integral under the parabola for the extent
  // that a wave can travel over a timestep
  //
  // Ip integrates to the right edge of a cell
  // Im integrates to the left edge of a cell

  Real sigma = std::abs(lam) * dtdx;

  if (lam <= 0.0_rt) {
    Ip = sp;
    Im = sm + 0.5_rt * sigma * (sp - sm + (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
  } else {
    Ip = sp - 0.5_rt * sigma * (sp - sm - (1.0_rt - (2.0_rt/3.0_rt) * sigma) * s6);
    Im = sm;
  }
}

#endif
