/** @file background.c Documented background module
 *
 * * Julien Lesgourgues, 17.04.2011
 * * routines related to ncdm written by T. Tram in 2011
 *
 * Deals with the cosmological background evolution.
 * This module has two purposes:
 *
 * - at the beginning, to initialize the background, i.e. to integrate
 *    the background equations, and store all background quantities
 *    as a function of conformal time inside an interpolation table.
 *
 * - to provide routines which allow other modules to evaluate any
 *    background quantity for a given value of the conformal time (by
 *    interpolating within the interpolation table), or to find the
 *    correspondence between redshift and conformal time.
 *
 *
 * The overall logic in this module is the following:
 *
 * 1. most background parameters that we will call {A}
 * (e.g. rho_gamma, ..) can be expressed as simple analytical
 * functions of a few variables that we will call {B} (in simplest
 * models, of the scale factor 'a'; in extended cosmologies, of 'a'
 * plus e.g. (phi, phidot) for quintessence, or some temperature for
 * exotic particles, etc...).
 *
 * 2. in turn, quantities {B} can be found as a function of conformal
 * time by integrating the background equations.
 *
 * 3. some other quantities that we will call {C} (like e.g. the
 * sound horizon or proper time) also require an integration with
 * respect to time, that cannot be inferred analytically from
 * parameters {B}.
 *
 * So, we define the following routines:
 *
 * - background_functions() returns all background
 *    quantities {A} as a function of quantities {B}.
 *
 * - background_solve() integrates the quantities {B} and {C} with
 *    respect to conformal time; this integration requires many calls
 *    to background_functions().
 *
 * - the result is stored in the form of a big table in the background
 *    structure. There is one column for conformal time 'tau'; one or
 *    more for quantities {B}; then several columns for quantities {A}
 *    and {C}.
 *
 * Later in the code, if we know the variables {B} and need some
 * quantity {A}, the quickest and most precise way is to call directly
 * background_functions() (for instance, in simple models, if we want
 * H at a given value of the scale factor). If we know 'tau' and want
 * any other quantity, we can call background_at_tau(), which
 * interpolates in the table and returns all values. Finally it can be
 * useful to get 'tau' for a given redshift 'z': this can be done with
 * background_tau_of_z(). So if we are somewhere in the code, knowing
 * z and willing to get background quantities, we should call first
 * background_tau_of_z() and then background_at_tau().
 *
 *
 * In order to save time, background_at_tau() can be called in three
 * modes: short_info, normal_info, long_info (returning only essential
 * quantities, or useful quantities, or rarely useful
 * quantities). Each line in the interpolation table is a vector whose
 * first few elements correspond to the short_info format; a larger
 * fraction contribute to the normal format; and the full vector
 * corresponds to the long format. The guideline is that short_info
 * returns only geometric quantities like a, H, H'; normal format
 * returns quantities strictly needed at each step in the integration
 * of perturbations; long_info returns quantities needed only
 * occasionally.
 *
 * In summary, the following functions can be called from other modules:
 *
 * -# background_init() at the beginning
 * -# background_at_tau(), background_tau_of_z() at any later time
 * -# background_free() at the end, when no more calls to the previous functions are needed
 */

#include "background.h"
// #include "gsl/gsl_sf_hyperg.h"
// #include "gsl/gsl_sf_gamma.h"

/**
 * Background quantities at given conformal time tau.
 *
 * Evaluates all background quantities at a given value of
 * conformal time by reading the pre-computed table and interpolating.
 *
 * @param pba           Input: pointer to background structure (containing pre-computed table)
 * @param tau           Input: value of conformal time
 * @param return_format Input: format of output vector (short, normal, long)
 * @param intermode     Input: interpolation mode (normal or closeby)
 * @param last_index    Input/Output: index of the previous/current point in the interpolation array (input only for closeby mode, output for both)
 * @param pvecback      Output: vector (assumed to be already allocated)
 * @return the error status
 */

int background_at_tau(
                      struct background *pba,
                      double tau,
                      short return_format,
                      short intermode,
                      int * last_index,
                      double * pvecback /* vector with argument pvecback[index_bg] (must be already allocated with a size compatible with return_format) */
                      ) {

  /** Summary: */

  /** - define local variables */

  /* size of output vector, controlled by input parameter return_format */
  int pvecback_size;

  /** - check that tau is in the pre-computed range */

  class_test(tau < pba->tau_table[0],
             pba->error_message,
             "out of range: tau=%e < tau_min=%e, you should decrease the precision parameter a_ini_over_a_today_default\n",tau,pba->tau_table[0]);

  class_test(tau > pba->tau_table[pba->bt_size-1],
             pba->error_message,
             "out of range: tau=%e > tau_max=%e\n",tau,pba->tau_table[pba->bt_size-1]);

  /** - deduce length of returned vector from format mode */

  if (return_format == pba->normal_info) {
    pvecback_size=pba->bg_size_normal;
  }
  else {
    if (return_format == pba->short_info) {
      pvecback_size=pba->bg_size_short;
    }
    else {
      pvecback_size=pba->bg_size;
    }
  }
  /** - interpolate from pre-computed table with array_interpolate()
      or array_interpolate_growing_closeby() (depending on
      interpolation mode) */

  if (intermode == pba->inter_normal) {
    class_call(array_interpolate_spline(
                                        pba->tau_table,
                                        pba->bt_size,
                                        pba->background_table,
                                        pba->d2background_dtau2_table,
                                        pba->bg_size,
                                        tau,
                                        last_index,
                                        pvecback,
                                        pvecback_size,
                                        pba->error_message),
               pba->error_message,
               pba->error_message);
  }
  if (intermode == pba->inter_closeby) {
    class_call(array_interpolate_spline_growing_closeby(
                                                        pba->tau_table,
                                                        pba->bt_size,
                                                        pba->background_table,
                                                        pba->d2background_dtau2_table,
                                                        pba->bg_size,
                                                        tau,
                                                        last_index,
                                                        pvecback,
                                                        pvecback_size,
                                                        pba->error_message),
               pba->error_message,
               pba->error_message);
  }

  return _SUCCESS_;
}

/**
 * Conformal time at given redshift.
 *
 * Returns tau(z) by interpolation from pre-computed table.
 *
 * @param pba Input: pointer to background structure
 * @param z   Input: redshift
 * @param tau Output: conformal time
 * @return the error status
 */

int background_tau_of_z(
                        struct background *pba,
                        double z,
                        double * tau
                        ) {

  /** Summary: */

  /** - define local variables */

  /* necessary for calling array_interpolate(), but never used */
  int last_index;

  /** - check that \f$ z \f$ is in the pre-computed range */
  class_test(z < pba->z_table[pba->bt_size-1],
             pba->error_message,
             "out of range: z=%e < z_min=%e\n",z,pba->z_table[pba->bt_size-1]);

  class_test(z > pba->z_table[0],
             pba->error_message,
             "out of range: a=%e > a_max=%e\n",z,pba->z_table[0]);

  /** - interpolate from pre-computed table with array_interpolate() */
  class_call(array_interpolate_spline(
                                      pba->z_table,
                                      pba->bt_size,
                                      pba->tau_table,
                                      pba->d2tau_dz2_table,
                                      1,
                                      z,
                                      &last_index,
                                      tau,
                                      1,
                                      pba->error_message),
             pba->error_message,
             pba->error_message);

  return _SUCCESS_;
}

/**
 * Background quantities at given \f$ a \f$.
 *
 * Function evaluating all background quantities which can be computed
 * analytically as a function of {B} parameters such as the scale factor 'a'
 * (see discussion at the beginning of this file). In extended
 * cosmological models, the pvecback_B vector contains other input parameters than
 * just 'a', e.g. (phi, phidot) for quintessence, some temperature of
 * exotic relics, etc...
 *
 * @param pba           Input: pointer to background structure
 * @param pvecback_B    Input: vector containing all {B} type quantities (scale factor, ...)
 * @param return_format Input: format of output vector
 * @param pvecback      Output: vector of background quantities (assumed to be already allocated)
 * @return the error status
 */

int background_functions(
                         struct background *pba,
                         double * pvecback_B, /* Vector containing all {B} quantities. */
                         short return_format,
                         double * pvecback /* vector with argument pvecback[index_bg] (must be already allocated with a size compatible with return_format) */
                         ) {

  /** Summary: */

  /** - define local variables */

  /* total density */
  double rho_tot;
  /* total pressure */
  double p_tot;
  /* total relativistic density */
  double rho_r;
  /* total non-relativistic density */
  double rho_m;
  /* scale factor relative to scale factor today */
  double a_rel;
  /* background ncdm quantities */
  double rho_ncdm,p_ncdm,pseudo_p_ncdm;
  /* index for n_ncdm species */
  int n_ncdm;

  int n;
  /* fluid's time-dependent equation of state parameter */
  double w_fld, dw_over_da, integral_fld;
  // TK added GDM here 
  /* GDM's time-dependent eq of state parameter */
  double w_gdm, dw_over_da_gdm, integral_gdm;
  /* scale factor */
  double a;
  /* scalar field quantities */
  double phi, phi_prime;

  /** - initialize local variables */
  a = pvecback_B[pba->index_bi_a];
  rho_tot = 0.;
  p_tot = 0.;
  rho_r=0.;
  rho_m=0.;
  a_rel = a / pba->a_today;

  class_test(a_rel <= 0.,
             pba->error_message,
             "a = %e instead of strictly positive",a_rel);

  /** - pass value of \f$ a\f$ to output */
  pvecback[pba->index_bg_a] = a;

  /** - compute each component's density and pressure */

  /* photons */
  pvecback[pba->index_bg_rho_g] = pba->Omega0_g * pow(pba->H0,2) / pow(a_rel,4);
  rho_tot += pvecback[pba->index_bg_rho_g];
  p_tot += (1./3.) * pvecback[pba->index_bg_rho_g];
  rho_r += pvecback[pba->index_bg_rho_g];

  /* baryons */
  pvecback[pba->index_bg_rho_b] = pba->Omega0_b * pow(pba->H0,2) / pow(a_rel,3);
  rho_tot += pvecback[pba->index_bg_rho_b];
  p_tot += 0;
  rho_m += pvecback[pba->index_bg_rho_b];

  /* cdm */
  if (pba->has_cdm == _TRUE_) {
    pvecback[pba->index_bg_rho_cdm] = pba->Omega0_cdm * pow(pba->H0,2) / pow(a_rel,3);
    rho_tot += pvecback[pba->index_bg_rho_cdm];
    p_tot += 0.;
    rho_m += pvecback[pba->index_bg_rho_cdm];
  }

  /* TK added GDM here */
  if (pba->has_gdm == _TRUE_) {
    /* get rho_gdm from vector of integrated variables */
    pvecback[pba->index_bg_rho_gdm] = pvecback_B[pba->index_bi_rho_gdm];
    /* get w_gdm from dedicated function */
    class_call(background_w_gdm(pba,a,&w_gdm,&dw_over_da_gdm,&integral_gdm), pba->error_message, pba->error_message);
    pvecback[pba->index_bg_w_gdm] = w_gdm;
    pvecback[pba->index_bg_dw_gdm] = dw_over_da_gdm; // /(1.+w_gdm); // ????? TK: why is this divided by 1+w ????

    rho_tot += pvecback[pba->index_bg_rho_gdm];
    p_tot += w_gdm * pvecback[pba->index_bg_rho_gdm];

    if ( (w_gdm > 0.33) && (w_gdm < 0.34) )rho_r += pvecback[pba->index_bg_rho_gdm];
    else if (w_gdm < 0.33 && w_gdm >= 0.)rho_m += pvecback[pba->index_bg_rho_gdm];
  }

  /* dcdm */
  if (pba->has_dcdm == _TRUE_) {
    /* Pass value of rho_dcdm to output */
    pvecback[pba->index_bg_rho_dcdm] = pvecback_B[pba->index_bi_rho_dcdm];
    rho_tot += pvecback[pba->index_bg_rho_dcdm];
    p_tot += 0.;
    rho_m += pvecback[pba->index_bg_rho_dcdm];
  }

  /* dr */
  if (pba->has_dr == _TRUE_) {
    /* Pass value of rho_dr to output */
    pvecback[pba->index_bg_rho_dr] = pvecback_B[pba->index_bi_rho_dr];
    rho_tot += pvecback[pba->index_bg_rho_dr];
    p_tot += (1./3.)*pvecback[pba->index_bg_rho_dr];
    rho_r += pvecback[pba->index_bg_rho_dr];
  }

  /* Scalar field */
  if (pba->has_scf == _TRUE_) {
    phi = pvecback_B[pba->index_bi_phi_scf];
    phi_prime = pvecback_B[pba->index_bi_phi_prime_scf];
    pvecback[pba->index_bg_phi_scf] = phi; // value of the scalar field phi
    pvecback[pba->index_bg_phi_prime_scf] = phi_prime; // value of the scalar field phi derivative wrt conformal time
    pvecback[pba->index_bg_V_scf] = V_scf(pba,phi); //V_scf(pba,phi); //write here potential as function of phi
    pvecback[pba->index_bg_dV_scf] = dV_scf(pba,phi); // dV_scf(pba,phi); //potential' as function of phi
    pvecback[pba->index_bg_ddV_scf] = ddV_scf(pba,phi); // ddV_scf(pba,phi); //potential'' as function of phi
    pvecback[pba->index_bg_rho_scf] = (phi_prime*phi_prime/(2*a*a) + V_scf(pba,phi))/3.; // energy of the scalar field. The field units are set automatically by setting the initial conditions
    pvecback[pba->index_bg_p_scf] =(phi_prime*phi_prime/(2*a*a) - V_scf(pba,phi))/3.; // pressure of the scalar field
    rho_tot += pvecback[pba->index_bg_rho_scf];
    p_tot += pvecback[pba->index_bg_p_scf];
    //divide relativistic & nonrelativistic (not very meaningful for oscillatory models)
    rho_r += 3.*pvecback[pba->index_bg_p_scf]; //field pressure contributes radiation
    rho_m += pvecback[pba->index_bg_rho_scf] - 3.* pvecback[pba->index_bg_p_scf]; //the rest contributes matter
    //printf(" a= %e, Omega_scf = %f, \n ",a_rel, pvecback[pba->index_bg_rho_scf]/rho_tot );
  }

  /* ncdm */
  if (pba->has_ncdm == _TRUE_) {

    /* Loop over species: */
    for(n_ncdm=0; n_ncdm<pba->N_ncdm; n_ncdm++){

      /* function returning background ncdm[n_ncdm] quantities (only
         those for which non-NULL pointers are passed) */
      class_call(background_ncdm_momenta(
                                         pba->q_ncdm_bg[n_ncdm],
                                         pba->w_ncdm_bg[n_ncdm],
                                         pba->q_size_ncdm_bg[n_ncdm],
                                         pba->M_ncdm[n_ncdm],
                                         pba->factor_ncdm[n_ncdm],
                                         1./a_rel-1.,
                                         NULL,
                                         &rho_ncdm,
                                         &p_ncdm,
                                         NULL,
                                         &pseudo_p_ncdm),
                 pba->error_message,
                 pba->error_message);

      pvecback[pba->index_bg_rho_ncdm1+n_ncdm] = rho_ncdm;
      rho_tot += rho_ncdm;
      pvecback[pba->index_bg_p_ncdm1+n_ncdm] = p_ncdm;
      p_tot += p_ncdm;
      pvecback[pba->index_bg_pseudo_p_ncdm1+n_ncdm] = pseudo_p_ncdm;

      /* (3 p_ncdm1) is the "relativistic" contribution to rho_ncdm1 */
      rho_r += 3.* p_ncdm;

      /* (rho_ncdm1 - 3 p_ncdm1) is the "non-relativistic" contribution
         to rho_ncdm1 */
      rho_m += rho_ncdm - 3.* p_ncdm;
    }
  }

  /* Lambda */
  if (pba->has_lambda == _TRUE_) {
    pvecback[pba->index_bg_rho_lambda] = pba->Omega0_lambda * pow(pba->H0,2);
    rho_tot += pvecback[pba->index_bg_rho_lambda];
    p_tot -= pvecback[pba->index_bg_rho_lambda];
  }

  /* fluid with w(a) and constant cs2 */
  if (pba->has_fld == _TRUE_) {
    for(n = 0 ; n<pba->n_fld ; n++){
      /* get rho_fld from vector of integrated variables */
      pvecback[pba->index_bg_rho_fld+n] = pvecback_B[pba->index_bi_rho_fld+n];

      /* get w_fld from dedicated function */
      class_call(background_w_fld(pba,a,&w_fld,&dw_over_da,&integral_fld,n), pba->error_message, pba->error_message);
      pvecback[pba->index_bg_w_fld+n] = w_fld;
      pvecback[pba->index_bg_dw_fld+n] = dw_over_da;
      // printf("a %e w_fld %e \n",a,w_fld);
      // Obsolete: at the beginning, we had here the analytic integral solution corresponding to the case w=w0+w1(1-a/a0):
      // pvecback[pba->index_bg_rho_fld] = pba->Omega0_fld * pow(pba->H0,2) / pow(a_rel,3.*(1.+pba->w0_fld+pba->wa_fld)) * exp(3.*pba->wa_fld*(a_rel-1.));
      // But now everthing is integrated numerically for a given w_fld(a) defined in the function background_w_fld.
      rho_tot += pvecback[pba->index_bg_rho_fld+n];
      p_tot += w_fld * pvecback[pba->index_bg_rho_fld+n];
    }

  }

  /* relativistic neutrinos (and all relativistic relics) */
  if (pba->has_ur == _TRUE_) {
    pvecback[pba->index_bg_rho_ur] = pba->Omega0_ur * pow(pba->H0,2) / pow(a_rel,4);
    rho_tot += pvecback[pba->index_bg_rho_ur];
    p_tot += (1./3.) * pvecback[pba->index_bg_rho_ur];
    rho_r += pvecback[pba->index_bg_rho_ur];

  }
  // if(pvecback[pba->index_bg_rho_fld]+pvecback[pba->index_bg_rho_cdm]>pvecback[pba->index_bg_rho_ur]+pvecback[pba->index_bg_rho_g])printf("a %e rho tot %e rho fld %e \n",a,rho_tot,pvecback[pba->index_bg_rho_fld]);

  /** - compute expansion rate H from Friedmann equation: this is the
      only place where the Friedmann equation is assumed. Remember
      that densities are all expressed in units of \f$ [3c^2/8\pi G] \f$, ie
      \f$ \rho_{class} = [8 \pi G \rho_{physical} / 3 c^2]\f$ */
  pvecback[pba->index_bg_H] = sqrt(rho_tot-pba->K/a/a);

  /** - compute derivative of H with respect to conformal time */
  pvecback[pba->index_bg_H_prime] = - (3./2.) * (rho_tot + p_tot) * a + pba->K/a;

  /** - compute relativistic density to total density ratio */
  pvecback[pba->index_bg_Omega_r] = rho_r / rho_tot;

  /** - compute other quantities in the exhaustive, redundant format */
  if (return_format == pba->long_info) {

    /** - compute critical density */
    pvecback[pba->index_bg_rho_crit] = rho_tot-pba->K/a/a;
    class_test(pvecback[pba->index_bg_rho_crit] <= 0.,
               pba->error_message,
               "rho_crit = %e instead of strictly positive",pvecback[pba->index_bg_rho_crit]);

    /** - compute Omega_m */
    pvecback[pba->index_bg_Omega_m] = rho_m / rho_tot;

    /* one can put other variables here */
    /*  */
    /*  */

  }

  return _SUCCESS_;

}


/**
 * Single place where the GDM equation of state is
 * defined. Parameters of the function are currently hard coded 
 * but can be added and passed through background srtuct
 * Generalisation to arbitrary functions should
 * be simple.
 *
 * @param pba            Input: pointer to background structure
 * @param a              Input: current value of scale factor
 * @param w_gdm          Output: equation of state parameter w_gdm(a)
 * @param dw_over_da_gdm Output: function dw_gdm/da
 * @param integral_gdm   Output: function \f$ \int_{a}^{a_0} da 3(1+w_{gdm})/a \f$
 * @return the error status
 */

int background_w_gdm(
                     struct background * pba,
                     double a,
                     double * w_gdm,
                     double * dw_over_da_gdm,
                     double * integral_gdm) {

 double  a_rel = a/ pba->a_today;
 double w,dw,intw;
    /** - first, define the function w(a) */
    // *w_gdm = pba->w0_gdm + pba->wa_gdm * (1. - a / pba->a_today);

    /** - then, give the corresponding analytic derivative dw/da (used
          by perturbation equations; we could compute it numerically,
          but with a loss of precision; as long as there is a simple
          analytic expression of the derivative of the previous
          function, let's use it! */

    // *dw_over_da_gdm = - pba->wa_gdm / pba->a_today;

    /** - finally, give the analytic solution of the following integral:
          \f$ \int_{a}^{a0} da 3(1+w_{fld})/a \f$. This is used in only
          one place, in the initial conditions for the background, and
          with a=a_ini. If your w(a) does not lead to a simple analytic
          solution of this integral, no worry: instead of writing
          something here, the best would then be to leave it equal to
          zero, and then in background_initial_conditions() you should
          implement a numerical calculation of this integral only for
          a=a_ini, using for instance Romberg integration. It should be
          fast, simple, and accurate enough. */

    // *integral_gdm = 3.*((1.+pba->w0_gdm+pba->wa_gdm)*log(pba->a_today/a) + pba->wa_gdm*(a/pba->a_today-1.));
    
    // printf("*integral_fld  %e w0_fld %e \n",*integral_fld,pba->w0_fld);
    /** note: of course you can generalise these formulas to anything,
        defining new parameters pba->w..._fld. Just remember that so
        far, HyRec explicitely assumes that w(a)= w0 + wa (1-a/a0); but
        Recfast does not assume anything */

    interpolate_w_gdm_at_a(pba,a_rel,&w,&dw);
    // if ( abs(w-1e-5) < 1e-10 ){
    //   printf("background\na = %e \t w_gdm = %e \n",a,w);
    //   printf("%e\n", (w-1e-5));
    // }

    if ( (w == -1.) || (w == 0.) ){
      w += 1e-10; // ?????? TK: eventually remove these? 
      // dw += 1e-10; // ?????? TK
    }
    // w=0.00001;
    // dw=0.0;
    *w_gdm = w; // why is this off by 1e-10? Is this to ensure it never exactly zero / -1 ? 
    *dw_over_da_gdm = dw;
    *integral_gdm=0; //will be computed later in background_init once and for all;

  // printf("a_rel %e %e %e %e\n",a_rel,*w_fld,*dw_over_da_fld,*integral_fld);

  return _SUCCESS_;
}


// This function needs to be called first to read all your array data for w_gdm and to store it correctly 
// other columns - dw, ddw, and dddw also need to be filled and saved 
int w_gdm_init( struct precision *ppr,
                struct background *pba
              ) {
  /** - --> second derivative with respect to tau of rho_w_gdm (in view of spline interpolation) */
  // This is set to zero by definition of spline interpolation 
  class_call(array_spline_table_line_to_line(pba->w_gdm_redshift_at_knot,
                                            pba->w_gdm_number_of_knots,
                                            pba->w_gdm_at_knot,
                                            pba->w_gdm_number_of_columns,
                                            0,
                                            2,
                                            _SPLINE_NATURAL_,
                                            pba->error_message),
            pba->error_message,
            pba->error_message);
  /** - --> first derivative with respect to tau of rho_w_gdm (using spline interpolation) */
  // first derivative calculated 
  class_call(array_derive_spline_table_line_to_line(pba->w_gdm_redshift_at_knot,
                                                     pba->w_gdm_number_of_knots,
                                                     pba->w_gdm_at_knot,
                                                     pba->w_gdm_number_of_columns,
                                                     0,
                                                     2,
                                                     1,
                                                     pba->error_message),
              pba->error_message,
              pba->error_message);

  /** - --> third derivative with respect to tau of rho_w_gdm (in view of spline interpolation) */
  // calculated
  class_call(array_spline_table_line_to_line(pba->w_gdm_redshift_at_knot,
                                               pba->w_gdm_number_of_knots,
                                               pba->w_gdm_at_knot,
                                               pba->w_gdm_number_of_columns,
                                               1,
                                               3,
                                               _SPLINE_NATURAL_,
                                               pba->error_message),
               pba->error_message,
               pba->error_message);

  /** - --> if necessary, fill a table of secondary derivative in view of spline interpolation */
  // save everything in a table so other functions can use it 
  if(pba->w_gdm_interpolation_is_linear == _FALSE_){
    class_call(array_spline_table_lines(pba->w_gdm_redshift_at_knot,
                                          pba->w_gdm_number_of_knots,
                                          pba->w_gdm_at_knot,
                                          pba->w_gdm_number_of_columns,
                                          pba->w_gdm_dd_at_knot,
                                          _SPLINE_EST_DERIV_,
                                          pba->error_message),
                  pba->error_message,
                  pba->error_message);
    }

    // for(int i=0;i<pba->w_gdm_number_of_knots;i++){
    //   printf("pba->w_gdm_at_knot %e pba->w_gdm_redshift_at_knot %e\n",pba->w_gdm_at_knot[i*pba->w_gdm_number_of_columns],pba->w_gdm_redshift_at_knot[i]);
    // }
   return _SUCCESS_;

}

// This function only tells romberg what to integrate - literally just gives it the integrand 
double integrand_gdm(struct background * pba,
                     double a,
                     int is_log){

  double tmp_w,tmp_dw,tmp_integral_gdm; //temporary storing quantities
  interpolate_w_gdm_at_a(pba,a,&tmp_w,&tmp_dw);

 if(is_log==_TRUE_)return 3*(1+tmp_w); //we integrate in log(a);
 else return 3*(1+tmp_w)/a;

}

// Uses romberg integration to get int_w_gdm, calls integrand_gdm 
int romberg_integrate_w_gdm( struct background * pba,
                             double /*lower limit*/ a,
                             double /*upper limit*/ b,
                             size_t max_steps,
                             double /*desired accuracy*/ acc,
                             double *intw_gdm,
                             int is_log){
   double R1[max_steps], R2[max_steps]; //buffers
   double *Rp = &R1[0], *Rc = &R2[0]; //Rp is previous row, Rc is current row
   double h = (b-a); //step size
   double x,xa=a,xb=b;
   size_t i;
   size_t j;
   if(is_log == _TRUE_){
     xa=pow(10,xa);
     xb=pow(10,xb);
   }
   Rp[0] = (integrand_gdm(pba,xa,is_log)+integrand_gdm(pba,xb,is_log))*h*.5; //first trapezoidal step
   // dump_row(0, Rp);

   for( i = 1; i < max_steps; ++i){
      h /= 2.;
      double c = 0;
      size_t ep = 1 << (i-1); //2^(n-1)
      for(j = 1; j <= ep; ++j){
         x = a+(2*j-1)*h;
         // printf("is_log %d x %e\n", is_log, x);
         if(is_log == _TRUE_){
           x=pow(10,x);
           // printf(" x %e j %d ep %d n", x,j,ep);
         }
         c += integrand_gdm(pba,x,is_log);
         // printf("c %e x %e j %d ep %d \n", c, x,j,ep);
      }
      Rc[0] = h*c + .5*Rp[0]; //R(i,0)

      for(j = 1; j <= i; ++j){
         double n_k = pow(4, j);
         Rc[j] = (n_k*Rc[j-1] - Rp[j-1])/(n_k-1); //compute R(i,j)
      }

      //Dump ith column of R, R[i,i] is the best estimate so far
      // dump_row(i, Rc);

      if(i > 1 && fabs(Rp[i-1]-Rc[i]) < acc){
         *intw_gdm = Rc[i-1];
         return _SUCCESS_;
      }

      //swap Rn and Rc as we only need the last row
      double *rt = Rp;
      Rp = Rc;
      Rc = rt;
   }
   // printf("Rp[max_steps-1] %e\n",Rp[max_steps-1]);
   *intw_gdm = Rp[max_steps-1]; //return our best guess

   return _SUCCESS_;
}


int interpolate_w_gdm_at_a(
                          struct background * pba,
                          double a,
                          double *w_gdm,
                          double *dw_gdm
                          ) {

  int last_index;
  double z;
  double epsilon = 1e-14;
  if (a!=0.) z = pba->a_today/a-1.;
  else z = pba->a_today/(epsilon)-1.;
  double result[pba->w_gdm_number_of_columns];
  int i,n;

  // if redhisft is in the range you want log interpolation and the table isn't already log, 
  // make it a log table and then set the flag of log table to true 
  if(z > pba->w_gdm_logz_interpolation_above_z && pba->w_gdm_table_is_log == _FALSE_ && pba->w_gdm_redshift_at_knot[pba->w_gdm_number_of_knots-1]>20){
      for(i = 0 ; i < pba->w_gdm_number_of_knots ; i++){
        pba->w_gdm_redshift_at_knot[i] = log10(pba->w_gdm_redshift_at_knot[i]);
      }
      // printf("a %e z %e pba->w_gdm_redshift_at_knot[pba->w_gdm_number_of_knots-1] %e %d \n", a,z,pba->w_gdm_redshift_at_knot[pba->w_gdm_number_of_knots-1],pba->w_gdm_table_is_log);
      pba->w_gdm_table_is_log = _TRUE_;
    }

  // if z in under the log interpolation range, and the table is log, 
  // un-log the table and set flag to false 
  else if(z<=pba->w_gdm_logz_interpolation_above_z && pba->w_gdm_table_is_log == _TRUE_ && pba->w_gdm_redshift_at_knot[pba->w_gdm_number_of_knots-1]  < 20){
    for(i = 0 ; i < pba->w_gdm_number_of_knots ; i++){
      pba->w_gdm_redshift_at_knot[i] = pow(10,pba->w_gdm_redshift_at_knot[i]);
    }
    // printf("a %e  z %e pba->w_gdm_redshift_at_knot[pba->w_gdm_number_of_knots-1] %e %d \n", a,z,pba->w_gdm_redshift_at_knot[pba->w_gdm_number_of_knots-1],pba->w_gdm_table_is_log);
    pba->w_gdm_table_is_log = _FALSE_;
  }

  // if we're in the z log interpolation range and we want log interpolation, 
  // convert the z we're looking for w at to log(z)
  if(z > pba->w_gdm_logz_interpolation_above_z && pba->w_gdm_table_is_log == _TRUE_){
    z=log10(z);
  }

  // either linearly interpolate if we're not in the log interpolate region 
  if(pba->w_gdm_interpolation_is_linear == _TRUE_){
   class_call(array_interpolate_linear(pba->w_gdm_redshift_at_knot,
                                    pba->w_gdm_number_of_knots,
                                    pba->w_gdm_at_knot,
                                    pba->w_gdm_number_of_columns,
                                    z,
                                    &last_index,
                                    result,
                                    pba->w_gdm_number_of_columns,
                                    pba->error_message),
           pba->error_message,
           pba->error_message);
  }
  // spline interpolate if we're in the log interpolate region 
  else{
    class_call(array_interpolate_spline(pba->w_gdm_redshift_at_knot,
                                        pba->w_gdm_number_of_knots,
                                        pba->w_gdm_at_knot,
                                        pba->w_gdm_dd_at_knot,
                                        pba->w_gdm_number_of_columns,
                                        z,
                                        &last_index,
                                        result,
                                        pba->w_gdm_number_of_columns,
                                        pba->error_message),
               pba->error_message,
               pba->error_message);
  }

  *w_gdm = result[0];
  *dw_gdm = result[1];
  // *intw_fld = 0;
  // printf("a %e z %e w_gdm %e dw_gdm %e ddwgdm %e  dddwgdm %e \n",a,z,*w_gdm,*dw_gdm,result[2],result[3]);
  return _SUCCESS_;
    }


/**
 * Single place where the fluid equation of state is
 * defined. Parameters of the function are passed through the
 * background structure. Generalisation to arbitrary functions should
 * be simple.
 *
 * @param pba            Input: pointer to background structure
 * @param a              Input: current value of scale factor
 * @param w_fld          Output: equation of state parameter w_fld(a)
 * @param dw_over_da_fld Output: function dw_fld/da
 * @param integral_fld   Output: function \f$ \int_{a}^{a_0} da 3(1+w_{fld})/a \f$
 * @return the error status
 */

int background_w_fld(
                     struct background * pba,
                     double a,
                     double * w_fld,
                     double * dw_over_da_fld,
                     double * integral_fld,
                     int n) {

 double  a_rel = a/ pba->a_today;
 double z = 1/a_rel-1;
 double center, width, wbefore, wafter;
 double w,dw,intw;
  if(pba->w_fld_parametrization == CPL){
    /** - first, define the function w(a) */
    *w_fld = pba->w0_fld + pba->wa_fld * (1. - a / pba->a_today);

    /** - then, give the corresponding analytic derivative dw/da (used
          by perturbation equations; we could compute it numerically,
          but with a loss of precision; as long as there is a simple
          analytic expression of the derivative of the previous
          function, let's use it! */
    *dw_over_da_fld = - pba->wa_fld / pba->a_today;

    /** - finally, give the analytic solution of the following integral:
          \f$ \int_{a}^{a0} da 3(1+w_{fld})/a \f$. This is used in only
          one place, in the initial conditions for the background, and
          with a=a_ini. If your w(a) does not lead to a simple analytic
          solution of this integral, no worry: instead of writing
          something here, the best would then be to leave it equal to
          zero, and then in background_initial_conditions() you should
          implement a numerical calculation of this integral only for
          a=a_ini, using for instance Romberg integration. It should be
          fast, simple, and accurate enough. */
    *integral_fld = 3.*((1.+pba->w0_fld+pba->wa_fld)*log(pba->a_today/a) + pba->wa_fld*(a/pba->a_today-1.));
    // printf("*integral_fld  %e w0_fld %e \n",*integral_fld,pba->w0_fld);
    /** note: of course you can generalise these formulas to anything,
        defining new parameters pba->w..._fld. Just remember that so
        far, HyRec explicitely assumes that w(a)= w0 + wa (1-a/a0); but
        Recfast does not assume anything */
  }
  else if(pba->w_fld_parametrization == pheno_axion){
    w = (pba->n_pheno_axion[n]-1)/(1+pba->n_pheno_axion[n]); //e.o.s. once the field starts oscillating
    *w_fld = (1+w)/(1+pow(pba->a_c[n]/a,3*(1+w)))-1+1e-15; //we add 1e-10 to avoid a crashing of the solver. Checked to be totally invisible.
    // *w_fld = (pow(a/ pba->a_today,6) - pow(pba->a_c/ pba->a_today,6))/(pow(a/ pba->a_today,6) + pow(pba->a_c/ pba->a_today,6));
    *dw_over_da_fld = 3*pow(a/pba->a_today,-1-3*(1+w))*pba->a_c[n]/ pba->a_today*(1+w)*(1+w)/pow((1 + pba->a_c[n]/pba->a_today*pow(a/ pba->a_today,-3*(1+w))),2);
    *integral_fld = -(3*(1 + w)*(3*w*log(a/pba->a_today) + log(pow(a/pba->a_today,3) + pow(pba->a_c[n]/ pba->a_today,3)*pow(pba->a_c[n]/a,3*w)))/(3 + 3*w));
    // printf("%e %e %e %e \n",a,*w_fld,*dw_over_da_fld,*integral_fld);
  }
  else if(pba->w_fld_parametrization == pheno_alternative){
    z = 1/a-1;
    center = 1/pba->a_c[n]-1;
    width = 2*center/200;//found to work well at capturing the sharp transition
    wbefore = -1+1e-15;
    wafter = (pba->n_pheno_axion[n]-1)/(1+pba->n_pheno_axion[n]);
    *w_fld = (wbefore - wafter)*(tanh((z - center)/width) + 1)/2 + wafter;
    // *w_fld = (pow(a/ pba->a_today,6) - pow(pba->a_c/ pba->a_today,6))/(pow(a/ pba->a_today,6) + pow(pba->a_c/ pba->a_today,6));
    *dw_over_da_fld = (wbefore - wafter)* (1-pow(tanh((z - center)/width),2))/(2*width);
    // printf("dw_over_da_fld %e\n", *dw_over_da_fld);
    *integral_fld = 0; //to be computed numerically
    // printf("%e %e %e %e \n",a,*w_fld,*dw_over_da_fld,*integral_fld);
  }
  // else if(pba->w_fld_parametrization == cos_axion){
  //   *w_fld = cos(pba->mu_axion)+0.5;
  //   *dw_over_da_fld = -sin();
  //   *integral_fld = log((pow(pba->a_today,6)+pow(pba->a_c/ pba->a_today,6))/(pow(a/ pba->a_today,6)+pow(pba->a_c/ pba->a_today,6)));
  //
  //   // printf("%e %e %e %e \n",a,*w_fld,*dw_over_da_fld,*integral_fld);
  // }
  else if(pba->w_fld_parametrization == w_free_function){
    if(pba->w_free_function_from_file == _TRUE_)interpolate_w_free_function_from_file_at_a(pba,a_rel,&w,&dw);
    else interpolate_w_free_function_at_a(pba,a_rel,&w,&dw);
    *w_fld = w;
    if(pba->w_free_function_file_is_ca2 == _TRUE_){
      *dw_over_da_fld = MAX(MIN(dw,pba->ca2_max),-1*pba->ca2_max);
    }
    else{
      *dw_over_da_fld = dw;
    }
    *integral_fld=0; //will be computed later in background_init once and for all;
  }
  // printf("a_rel %e %e %e %e\n",a_rel,*w_fld,*dw_over_da_fld,*integral_fld);


  return _SUCCESS_;
}


int w_free_function_init( struct precision *ppr,
                          struct background *pba
                         ) {
     FILE * fA = NULL;
     char line[_LINE_LENGTH_MAX_];
     char * left;

     /* BEGIN: New variables related to the use of an external code to calculate the annihilation coefficients */
     char arguments[_ARGUMENT_LENGTH_MAX_];
     char command_with_arguments[2*_ARGUMENT_LENGTH_MAX_];
     int status;
     /* END */

     int num_lines=0;
     int array_line=0;

     if(pba->w_free_function_from_file==_TRUE_){

       class_open(fA,ppr->w_free_function_file, "r",pba->error_message);
       pba->w_free_function_number_of_columns =  1;
         /* go through each line */
         while (fgets(line,_LINE_LENGTH_MAX_-1,fA) != NULL) {
           /* eliminate blank spaces at beginning of line */
           left=line;
           while (left[0]==' ') {
             left++;
           }

           /* check that the line is neither blank nor a comment. In
              ASCII, left[0]>39 means that first non-blank charachter might
              be the beginning of some data (it is not a newline, a #, a %,
              etc.) */
           if (left[0] > 39) {

             /* if the line contains data, we must interprete it. If
                num_lines == 0 , the current line must contain
                its value. Otherwise, it must contain (z,w,dw). */
             if (num_lines == 0) {

               /* read num_lines, infer size of arrays and allocate them */
               class_test(sscanf(line,"%d",&num_lines) != 1,
                          pba->error_message,
                          "could not read value of parameters num_lines in file %s\n",ppr->w_free_function_file);
               class_alloc(pba->w_free_function_redshift_at_knot,num_lines*sizeof(double),pba->error_message);
               class_alloc(pba->w_free_function_at_knot,num_lines*sizeof(double),pba->error_message);
               class_alloc(pba->w_free_function_d_at_knot,num_lines*sizeof(double),pba->error_message);
               class_alloc(pba->w_free_function_dd_at_knot,num_lines*sizeof(double),pba->error_message);
               class_alloc(pba->w_free_function_ddd_at_knot,num_lines*sizeof(double),pba->error_message);
               pba->w_free_function_number_of_knots = num_lines;


               array_line=0;

             }
             else {

               /* read coefficients */
               class_test(sscanf(line,"%lg %lg %lg ",
                                 &(pba->w_free_function_redshift_at_knot[array_line]),
                                 &(pba->w_free_function_at_knot[array_line]),
                                 &(pba->w_free_function_d_at_knot[array_line])) != 3,
                          pba->error_message,
                          "could not read value of parameters coefficients in file %s\n",ppr->w_free_function_file);
               array_line ++;
             }
           }
         }
         fclose(fA);
           /* spline in one dimension */
           class_call(array_spline_table_lines(pba->w_free_function_redshift_at_knot,
                                               num_lines,
                                               pba->w_free_function_at_knot,
                                               1,
                                               pba->w_free_function_dd_at_knot,
                                               _SPLINE_NATURAL_,
                                               pba->error_message),
                      pba->error_message,
                      pba->error_message);

           class_call(array_spline_table_lines(pba->w_free_function_redshift_at_knot,
                                               num_lines,
                                               pba->w_free_function_d_at_knot,
                                               1,
                                               pba->w_free_function_ddd_at_knot,
                                               _SPLINE_NATURAL_,
                                               pba->error_message),
                      pba->error_message,
                      pba->error_message);


     }
     else{
       /** - --> second derivative with respect to tau of rho_w_free_function (in view of spline interpolation) */
       class_call(array_spline_table_line_to_line(pba->w_free_function_redshift_at_knot,
                                                  pba->w_free_function_number_of_knots,
                                                  pba->w_free_function_at_knot,
                                                  pba->w_free_function_number_of_columns,
                                                  0,
                                                  2,
                                                  _SPLINE_NATURAL_,
                                                  pba->error_message),
                  pba->error_message,
                  pba->error_message);
       /** - --> first derivative with respect to tau of rho_w_free_function (using spline interpolation) */
       class_call(array_derive_spline_table_line_to_line(pba->w_free_function_redshift_at_knot,
                                                         pba->w_free_function_number_of_knots,
                                                         pba->w_free_function_at_knot,
                                                         pba->w_free_function_number_of_columns,
                                                         0,
                                                         2,
                                                         1,
                                                         pba->error_message),
                  pba->error_message,
                  pba->error_message);

        /** - --> third derivative with respect to tau of rho_w_free_function (in view of spline interpolation) */
        class_call(array_spline_table_line_to_line(pba->w_free_function_redshift_at_knot,
                                                   pba->w_free_function_number_of_knots,
                                                   pba->w_free_function_at_knot,
                                                   pba->w_free_function_number_of_columns,
                                                   1,
                                                   3,
                                                   _SPLINE_NATURAL_,
                                                   pba->error_message),
                   pba->error_message,
                   pba->error_message);

        /** - --> if necessary, fill a table of secondary derivative in view of spline interpolation */
        if(pba->w_free_function_interpolation_is_linear == _FALSE_){
          class_call(array_spline_table_lines(pba->w_free_function_redshift_at_knot,
                                              pba->w_free_function_number_of_knots,
                                              pba->w_free_function_at_knot,
                                              pba->w_free_function_number_of_columns,
                                              pba->w_free_function_dd_at_knot,
                                              _SPLINE_EST_DERIV_,
                                              pba->error_message),
                      pba->error_message,
                      pba->error_message);
        }

     }

        // for(int i=0;i<pba->w_free_function_number_of_knots*pba->w_free_function_number_of_columns;i++){
        //   printf("pba->w_free_function_at_knot %e\n",pba->w_free_function_at_knot[i]);
        // }
     return _SUCCESS_;
}

double integrand_fld_free_function(struct background * pba,
                                   double a,
                                   int is_log,
                                   int n_fld){

 double tmp_w,tmp_dw,tmp_integral_fld; //temporary storing quantities
 if(pba->w_fld_parametrization == w_free_function){
   if(pba->w_free_function_from_file == _TRUE_)interpolate_w_free_function_from_file_at_a(pba,a,&tmp_w,&tmp_dw);
   else interpolate_w_free_function_at_a(pba,a,&tmp_w,&tmp_dw);
 }
 else if(pba->w_fld_parametrization == pheno_alternative){
   class_call(background_w_fld(pba,a,&tmp_w,&tmp_dw,&tmp_integral_fld,n_fld), pba->error_message, pba->error_message);
 }


 if(is_log==_TRUE_)return 3*(1+tmp_w); //we integrate in loga;
 else return 3*(1+tmp_w)/a;
}
// int simpson_integrate_w_free_function(struct background * pba,
//                                          double /*lower limit*/ a,
//                                          double /*upper limit*/ b,
//                                          size_t max_steps,
//                                          // double /*desired accuracy*/ acc,
//                                          double *intw_fld){
//    double h = (b-a)/6, s;
//    int i=1;
//    s = (integrand_fld_free_function(pba,pow(10,a),is_)+integrand_fld_free_function(pba,pow(10,b)));
//    while(i<max_steps){
//      s += 4 * integrand_fld_free_function(pba,pow(10,(a + i * h)));
//      i+=2;
//    }
//    i=2;
//    while(i<max_steps-1){
//      s += 2 * integrand_fld_free_function(pba,pow(10,(a + i * h)));
//      i+=2;
//    }
//
//    *intw_fld = s*h/3;
//    return _SUCCESS_;
// }
int romberg_integrate_w_free_function(struct background * pba,
                                         double /*lower limit*/ a,
                                         double /*upper limit*/ b,
                                         size_t max_steps,
                                         double /*desired accuracy*/ acc,
                                         double *intw_fld,
                                         int is_log,
                                         int n){
   double R1[max_steps], R2[max_steps]; //buffers
   double *Rp = &R1[0], *Rc = &R2[0]; //Rp is previous row, Rc is current row
   double h = (b-a); //step size
   double x,xa=a,xb=b;
   size_t i;
   size_t j;
   if(is_log == _TRUE_){
     xa=pow(10,xa);
     xb=pow(10,xb);
   }
   Rp[0] = (integrand_fld_free_function(pba,xa,is_log,n)+integrand_fld_free_function(pba,xb,is_log,n))*h*.5; //first trapezoidal step
   // dump_row(0, Rp);

   for( i = 1; i < max_steps; ++i){
      h /= 2.;
      double c = 0;
      size_t ep = 1 << (i-1); //2^(n-1)
      for(j = 1; j <= ep; ++j){
         x = a+(2*j-1)*h;
         // printf("is_log %d x %e\n", is_log, x);
         if(is_log == _TRUE_){
           x=pow(10,x);
           // printf(" x %e j %d ep %d n", x,j,ep);
         }
         c += integrand_fld_free_function(pba,x,is_log,n);
         // printf("c %e x %e j %d ep %d \n", c, x,j,ep);
      }
      Rc[0] = h*c + .5*Rp[0]; //R(i,0)

      for(j = 1; j <= i; ++j){
         double n_k = pow(4, j);
         Rc[j] = (n_k*Rc[j-1] - Rp[j-1])/(n_k-1); //compute R(i,j)
      }

      //Dump ith column of R, R[i,i] is the best estimate so far
      // dump_row(i, Rc);

      if(i > 1 && fabs(Rp[i-1]-Rc[i]) < acc){
         *intw_fld = Rc[i-1];
         return _SUCCESS_;
      }

      //swap Rn and Rc as we only need the last row
      double *rt = Rp;
      Rp = Rc;
      Rc = rt;
   }
   // printf("Rp[max_steps-1] %e\n",Rp[max_steps-1]);
   *intw_fld = Rp[max_steps-1]; //return our best guess

   return _SUCCESS_;
}

int interpolate_w_free_function_from_file_at_a(
                                               struct background * pba,
                                               double a,
                                               double *w_fld,
                                               double *dw_fld
                                               ) {

  int last_index,i;
  double z=1e14;
  if(a!=0) z=1./a-1;
  double tmp_w,tmp_dw;
  // printf("z %e\n",z);
  // if(z==1e14)for(i = 0;i<pba->w_free_function_number_of_knots;i++){
  //   printf("%e %e %e\n",pba->w_free_function_redshift_at_knot[i],pba->w_free_function_at_knot[i],pba->w_free_function_d_at_knot[i]);
  // }

  if(pba->w_free_function_interpolation_is_linear == _TRUE_){
      class_call(array_interpolate_linear(pba->w_free_function_redshift_at_knot,
                                       pba->w_free_function_number_of_knots,
                                       pba->w_free_function_at_knot,
                                       1,
                                       z,
                                       &last_index,
                                       &tmp_w,
                                       1,
                                       pba->error_message),
              pba->error_message,
              pba->error_message);
      class_call(array_interpolate_linear(pba->w_free_function_redshift_at_knot,
                                       pba->w_free_function_number_of_knots,
                                       pba->w_free_function_d_at_knot,
                                       1,
                                       z,
                                       &last_index,
                                       &tmp_dw,
                                       1,
                                       pba->error_message),
              pba->error_message,
              pba->error_message);
    }
    else{
      class_call(array_interpolate_spline(pba->w_free_function_redshift_at_knot,
                                          pba->w_free_function_number_of_knots,
                                          pba->w_free_function_at_knot,
                                          pba->w_free_function_dd_at_knot,
                                          1,
                                          z,
                                          &last_index,
                                          w_fld,
                                          1,
                                          pba->error_message),
                 pba->error_message,
                 pba->error_message);

      class_call(array_interpolate_spline(pba->w_free_function_redshift_at_knot,
                                          pba->w_free_function_number_of_knots,
                                          pba->w_free_function_d_at_knot,
                                          pba->w_free_function_ddd_at_knot,
                                          1,
                                          z,
                                          &last_index,
                                          dw_fld,
                                          1,
                                          pba->error_message),
                 pba->error_message,
                 pba->error_message);
    }
  *w_fld=tmp_w;
  *dw_fld=tmp_dw;
        // printf("pba->w_free_function_number_of_knots %d\n", pba->w_free_function_number_of_knots);
  // printf("z %e %e %e pba->w_free_function_number_of_knots %d \n",z,*w_fld,*dw_fld,pba->w_free_function_number_of_knots);
  return _SUCCESS_;

}

int interpolate_w_free_function_at_a(
                          struct background * pba,
                          double a,
                          double *w_fld,
                          double *dw_fld
                          ) {

  int last_index;
  double z;
  double epsilon = 1e-14;
  if (a!=0.) z = pba->a_today/a-1.;
  else z = pba->a_today/(epsilon)-1.;
  double result[pba->w_free_function_number_of_columns];
  int i,n;

  if(z > pba->w_free_function_logz_interpolation_above_z && pba->w_free_function_table_is_log == _FALSE_ && pba->w_free_function_redshift_at_knot[pba->w_free_function_number_of_knots-1]>20){
      for(i = 0 ; i < pba->w_free_function_number_of_knots ; i++){
        pba->w_free_function_redshift_at_knot[i] = log10(pba->w_free_function_redshift_at_knot[i]);
      }
      // printf("a %e z %e pba->w_free_function_redshift_at_knot[pba->w_free_function_number_of_knots-1] %e %d \n", a,z,pba->w_free_function_redshift_at_knot[pba->w_free_function_number_of_knots-1],pba->w_free_function_table_is_log);
      pba->w_free_function_table_is_log = _TRUE_;
    }

  else if(z<=pba->w_free_function_logz_interpolation_above_z && pba->w_free_function_table_is_log == _TRUE_ && pba->w_free_function_redshift_at_knot[pba->w_free_function_number_of_knots-1]  < 20){
    for(i = 0 ; i < pba->w_free_function_number_of_knots ; i++){
      pba->w_free_function_redshift_at_knot[i] = pow(10,pba->w_free_function_redshift_at_knot[i]);
    }
    // printf("a %e  z %e pba->w_free_function_redshift_at_knot[pba->w_free_function_number_of_knots-1] %e %d \n", a,z,pba->w_free_function_redshift_at_knot[pba->w_free_function_number_of_knots-1],pba->w_free_function_table_is_log);
    pba->w_free_function_table_is_log = _FALSE_;
  }

  if(z > pba->w_free_function_logz_interpolation_above_z && pba->w_free_function_table_is_log == _TRUE_){
    z=log10(z);
  }



   if(pba->w_free_function_interpolation_is_linear == _TRUE_){
     class_call(array_interpolate_linear(pba->w_free_function_redshift_at_knot,
                                      pba->w_free_function_number_of_knots,
                                      pba->w_free_function_at_knot,
                                      pba->w_free_function_number_of_columns,
                                      z,
                                      &last_index,
                                      result,
                                      pba->w_free_function_number_of_columns,
                                      pba->error_message),
             pba->error_message,
             pba->error_message);
    }
    else{
      class_call(array_interpolate_spline(pba->w_free_function_redshift_at_knot,
                                          pba->w_free_function_number_of_knots,
                                          pba->w_free_function_at_knot,
                                          pba->w_free_function_dd_at_knot,
                                          pba->w_free_function_number_of_columns,
                                          z,
                                          &last_index,
                                          result,
                                          pba->w_free_function_number_of_columns,
                                          pba->error_message),
                 pba->error_message,
                 pba->error_message);
    }


  *w_fld = result[0];
  *dw_fld = result[1];
  // *intw_fld = 0;
  // printf("a %e z %e w_fld %e dw_fld %e ddwfld %e  dddwfld %e \n",a,z,*w_fld,*dw_fld,result[2],result[3]);
  return _SUCCESS_;
}



/**
 * Initialize the background structure, and in particular the
 * background interpolation table.
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input/Output: pointer to initialized background structure
 * @return the error status
 */

int background_init(
                    struct precision * ppr,
                    struct background * pba
                    ) {

  /** Summary: */

  /** - define local variables */
  int n_ncdm;
  double rho_ncdm_rel,rho_nu_rel;
  double Neff;
  double w_gdm, dw_over_da_gdm, integral_gdm; // TK added 
  double w_fld, dw_over_da, integral_fld;
  int filenum=0;
  /* vector of all background quantities */
  double * pvecback;
  /* necessary for calling array_interpolate(), but never used */
  int last_index=0;
  /* parameters required to get m_fld when playing with axion */
  double tau_of_ac,p,F1_1,F1_2,cos_initial,sin_initial,wn,Eac,Omega_fld_ac,Omega0_fld,signArg,xc,Gac,f;
  int i;
  double n;

  /** - in verbose mode, provide some information */
  if (pba->background_verbose > 0) {
    printf("Running CLASS version %s\n",_VERSION_);
    printf("Computing background\n");

    /* below we want to inform the user about ncdm species*/
    if (pba->N_ncdm > 0) {

      Neff = pba->Omega0_ur/7.*8./pow(4./11.,4./3.)/pba->Omega0_g;

      /* loop over ncdm species */
      for (n_ncdm=0;n_ncdm<pba->N_ncdm; n_ncdm++) {

        /* inform if p-s-d read in files */
        if (pba->got_files[n_ncdm] == _TRUE_) {
          printf(" -> ncdm species i=%d read from file %s\n",n_ncdm+1,pba->ncdm_psd_files+filenum*_ARGUMENT_LENGTH_MAX_);
          filenum++;
        }

        /* call this function to get rho_ncdm */
        background_ncdm_momenta(pba->q_ncdm_bg[n_ncdm],
                                pba->w_ncdm_bg[n_ncdm],
                                pba->q_size_ncdm_bg[n_ncdm],
                                0.,
                                pba->factor_ncdm[n_ncdm],
                                0.,
                                NULL,
                                &rho_ncdm_rel,
                                NULL,
                                NULL,
                                NULL);

        /* inform user of the contribution of each species to
           radiation density (in relativistic limit): should be
           between 1.01 and 1.02 for each active neutrino species;
           evaluated as rho_ncdm/rho_nu_rel where rho_nu_rel is the
           density of one neutrino in the instantaneous decoupling
           limit, i.e. assuming T_nu=(4/11)^1/3 T_gamma (this comes
           from the definition of N_eff) */
        rho_nu_rel = 56.0/45.0*pow(_PI_,6)*pow(4.0/11.0,4.0/3.0)*_G_/pow(_h_P_,3)/pow(_c_,7)*
          pow(_Mpc_over_m_,2)*pow(pba->T_cmb*_k_B_,4);

        printf(" -> ncdm species i=%d sampled with %d (resp. %d) points for purpose of background (resp. perturbation) integration. In the relativistic limit it gives Delta N_eff = %g\n",
               n_ncdm+1,
               pba->q_size_ncdm_bg[n_ncdm],
               pba->q_size_ncdm[n_ncdm],
               rho_ncdm_rel/rho_nu_rel);

        Neff += rho_ncdm_rel/rho_nu_rel;

      }

      printf(" -> total N_eff = %g (sumed over ultra-relativistic and ncdm species)\n",Neff);

    }
  }

  /** - if shooting failed during input, catch the error here */
  class_test(pba->shooting_failed == _TRUE_,
             pba->error_message,
             "Shooting failed, try optimising input_get_guess(). Error message:\n\n%s",
             pba->shooting_error);

  /** - assign values to all indices in vectors of background quantities with background_indices()*/
  class_call(background_indices(pba),
             pba->error_message,
             pba->error_message);

  /** - control that cosmological parameter values make sense */

  /* H0 in Mpc^{-1} */
  /* Many users asked for this test to be supressed. It is commented out. */
  /*class_test((pba->H0 < _H0_SMALL_)||(pba->H0 > _H0_BIG_),
             pba->error_message,
             "H0=%g out of bounds (%g<H0<%g) \n",pba->H0,_H0_SMALL_,_H0_BIG_);*/

  class_test(fabs(pba->h * 1.e5 / _c_  / pba->H0 -1.)>ppr->smallest_allowed_variation,
             pba->error_message,
             "inconsistency between Hubble and reduced Hubble parameters: you have H0=%f/Mpc=%fkm/s/Mpc, but h=%f",pba->H0,pba->H0/1.e5* _c_,pba->h);

  /* T_cmb in K */
  /* Many users asked for this test to be supressed. It is commented out. */
  /*class_test((pba->T_cmb < _TCMB_SMALL_)||(pba->T_cmb > _TCMB_BIG_),
             pba->error_message,
             "T_cmb=%g out of bounds (%g<T_cmb<%g)",pba->T_cmb,_TCMB_SMALL_,_TCMB_BIG_);*/

  /* Omega_k */
  /* Many users asked for this test to be supressed. It is commented out. */
  /*class_test((pba->Omega0_k < _OMEGAK_SMALL_)||(pba->Omega0_k > _OMEGAK_BIG_),
             pba->error_message,
             "Omegak = %g out of bounds (%g<Omegak<%g) \n",pba->Omega0_k,_OMEGAK_SMALL_,_OMEGAK_BIG_);*/

  // TK added GDM here 
  /* GDM equation of state */
  if (pba->has_gdm == _TRUE_) {
      w_gdm_init(ppr,pba);
      class_call(background_w_gdm(pba,0,&w_gdm,&dw_over_da_gdm,&integral_gdm), pba->error_message, pba->error_message); 
    }

  /* fluid equation of state */
  if (pba->has_fld == _TRUE_) {
    if(pba->w_fld_parametrization == w_free_function){
      w_free_function_init(ppr,pba);
    }
    class_call(background_w_fld(pba,0,&w_fld,&dw_over_da,&integral_fld,0), pba->error_message, pba->error_message);

    class_test(w_fld >= 1./3.,
               pba->error_message,
               "Your choice for w(a--->0)=%g is suspicious, since it is bigger than -1/3 there cannot be radiation domination at early times\n",
               w_fld);


  }

  if(pba->w_fld_parametrization == pheno_axion){

    if(pba->axion_is_mu_and_alpha == _TRUE_){
      printf("Here %e\n",pba->a_c[0]);
      //We have passed mu and alpha so we need to assign Omega_ac, ac and omega_n. Only works with a single fluid.
      if(pba->a_c[0]<(pba->Omega0_g+pba->Omega0_ur)/(pba->Omega0_b+pba->Omega0_cdm)){
        p = 1./2;
      }
      else{
        p = 2./3;
      }


      cos_initial = cos(pba->Theta_initial_fld[0]);
      sin_initial = sin(pba->Theta_initial_fld[0]);
      n = pba->n_pheno_axion[0];

      if((n-1+n*cos_initial)>0){
          signArg = 1.;
      }
      else{
          signArg = -1.;
      }

      wn = (n-1)/(n+1);



     // printf(" (pba->Omega0_g+pba->Omega0_ur)*pow(pba->a_c[0],-4)+(pba->Omega0_b+pba->Omega0_cdm)*pow(pba->a_c[0],-3) %e pba->Omega_fld_ac[0] %e\n",(pba->Omega0_g+pba->Omega0_ur)*pow(pba->a_c[0],-4)+(pba->Omega0_b+pba->Omega0_cdm)*pow(pba->a_c[0],-3),pba->Omega_fld_ac[0]);
      pba->Omega_many_fld[0] = 2*pba->Omega_fld_ac[0]/(pow(pba->a_c[0],-3*wn-3)+1);
      pba->Omega0_lambda -= pba->Omega_many_fld[0]; // we want a flat universe today.
      // pba->omega_axion[0] = pba->H0*sqrt(_PI_)*pow(2,-(n*n+1)/(2*n))*pow(2*pow(pba->a_c[0],3*(1+wn))*pba->Omega_fld_ac[0]/(1+pow(pba->a_c[0],3*(1+wn))),(n-1)/(2*n))*gsl_sf_gamma((n+1.)/(2*n))*pow(pba->m_fld[0]*pba->alpha_fld[0],1./n)/(pba->alpha_fld[0]*gsl_sf_gamma(1+1./(2*n)));
      // printf("pba->Omega_many_fld[0] %e (pow(pba->a_c[0],-3*(wn+1))+1) %e wn %e\n",pba->Omega_many_fld[0],pow(pba->a_c[0],-3*wn-3)+1);
    }
    else{
      for(i =0 ;i<pba->n_fld;i++){

        if(pba->a_c[i]<(pba->Omega0_g+pba->Omega0_ur)/(pba->Omega0_b+pba->Omega0_cdm)){
          p = 1./2;
        }
        else{
          p = 2./3;
        }

          if((n-1+n*cos_initial)>0){
              signArg = 1.;
          }
          else{
              signArg =-1.;
          }


        cos_initial = cos(pba->Theta_initial_fld[i]);
        sin_initial = sin(pba->Theta_initial_fld[i]);
        n = pba->n_pheno_axion[i];
        wn = (n-1)/(n+1);


        if(pba->Omega0_fld!=0 || pba->Omega_many_fld[i] != 0) Omega0_fld = pba->Omega0_fld;
        else Omega0_fld = pba->Omega_many_fld[i];
        Omega_fld_ac = Omega0_fld/2*(pow(pba->a_c[i],-3*(wn+1))+1);

        // F1_1 = gsl_sf_hyperg_0F1(1./2*(1+3*p),signArg);
        // F1_2 = gsl_sf_hyperg_0F1(3./2*(1+p),signArg);
        Eac = sqrt((pba->Omega0_g+pba->Omega0_ur)*pow(pba->a_c[i],-4)+(pba->Omega0_b+pba->Omega0_cdm)*pow(pba->a_c[i],-3)+pba->Omega0_lambda+pba->Omega_fld_ac[i]);

        xc = p/Eac;
        f = 7./8;
        // pba->m_fld[i] = sqrt(4/n*pow(Eac,2)*pow(pow(1-cos_initial,n-1)*fabs(1-n*cos_initial-n),-1)); //OLD
        // pba->alpha_fld[i] =sqrt(3*pba->Omega_fld_ac/pow(pba->m_fld[i],2)*pow(
        //   pow(1-cos(pba->Theta_initial_fld[i]+(-1+F1_1)*sin_initial/(-1+n+n*cos_initial)),n)
        //  +2*n*pow(1-cos_initial,n-1)*pow(F1_2*sin_initial,2)/(pow(1+3*p,2)*fabs(1-n-n*cos_initial)),-1));
        // pba->omega_axion[i] = pba->H0*sqrt(_PI_)*pow(2,-(n*n+1)/(2*n))*pow(2*pow(pba->a_c[i],3*(1+wn))*Omega_fld_ac/(1+pow(pba->a_c[i],3*(1+wn))),(n-1)/(2*n))*gsl_sf_gamma((n+1.)/(2*n))*pow(pba->m_fld[i]*pba->alpha_fld[i],1./n)/(pba->alpha_fld[i]*gsl_sf_gamma(1+1./(2*n)));


        pba->m_fld[i] = pow(1-cos_initial,(1.-n)/2.)*sqrt((1-f)*(6*p+2)*pba->Theta_initial_fld[i]/(n*sin_initial))/xc;
        pba->alpha_fld[i] = sqrt(6 * pba->Omega_fld_ac[i])/pba->m_fld[i]/pow(1-cos_initial,n/2);

        // Gac =sqrt(_PI_)*gsl_sf_gamma((n+1.)/(2*n))/gsl_sf_gamma(1+1./(2*n))*pow(2,-(n*n+1)/(2*n))*pow(3,0.5*(1./n-1))
        // *pow(pba->a_c[i],3-6./(1+n))*pow(pow(pba->a_c[i],6*n/(1+n))+1,0.5*(1./n-1));

        pba->omega_axion[i] = pba->H0*pba->m_fld[i]*pow(1-cos_initial,0.5*(n-1))*Gac;


        // printf("pba->m_fld %e pba->alpha_fld %e pba->omega_axion[i] %e Gac  %e  \n", pba->m_fld[i],pba->alpha_fld[i],pba->omega_axion[i]*pow(pba->a_c[i],-3*wn)*pba->a_c[i],Gac);
      }
    }





  }
  /* in verbose mode, inform the user about the value of the ncdm
     masses in eV and about the ratio [m/omega_ncdm] in eV (the usual
     93 point something)*/
  if ((pba->background_verbose > 0) && (pba->has_ncdm == _TRUE_)) {
    for (n_ncdm=0; n_ncdm < pba->N_ncdm; n_ncdm++) {
      printf(" -> non-cold dark matter species with i=%d has m_i = %e eV (so m_i / omega_i =%e eV)\n",
             n_ncdm+1,
             pba->m_ncdm_in_eV[n_ncdm],
             pba->m_ncdm_in_eV[n_ncdm]*pba->deg_ncdm[n_ncdm]/pba->Omega0_ncdm[n_ncdm]/pba->h/pba->h);
    }
  }

  /* check other quantities which would lead to segmentation fault if zero */
  class_test(pba->a_today <= 0,
             pba->error_message,
             "input a_today = %e instead of strictly positive",pba->a_today);

  class_test(_Gyr_over_Mpc_ <= 0,
             pba->error_message,
             "_Gyr_over_Mpc = %e instead of strictly positive",_Gyr_over_Mpc_);

  /** - this function integrates the background over time, allocates
      and fills the background table */
  class_call(background_solve(ppr,pba),
             pba->error_message,
             pba->error_message);






  return _SUCCESS_;

}

/**
 * Free all memory space allocated by background_init().
 *
 *
 * @param pba Input: pointer to background structure (to be freed)
 * @return the error status
 */

int background_free(
                    struct background *pba
                    ) {
  int err;

  free(pba->tau_table);
  free(pba->z_table);
  free(pba->d2tau_dz2_table);
  free(pba->background_table);
  free(pba->d2background_dtau2_table);
  // if(pba->w_fld_parametrization == w_free_function){
  //   free(pba->w_free_function_at_knot);
  //   free(pba->w_free_function_value_at_knot);
  //   free(pba->w_free_function_redshift_at_knot);
  //   if(pba->w_free_function_interpolation_is_linear == _FALSE_)free(pba->w_free_function_dd_at_knot);
  // }

  if(pba->has_gdm == _TRUE_){
    free(pba->w_gdm_at_knot);
    free(pba->w_gdm_value_at_knot);
    free(pba->w_gdm_redshift_at_knot);
    if(pba->w_gdm_interpolation_is_linear == _FALSE_)free(pba->w_gdm_dd_at_knot);
  }
  // TK need to free these.
  // Freed them here, but perturbation was still relying on class_call(background_w_gdm) which in turn used these arrays
  // Now perturbations points to the saved pba->index_bg_w_gdm and so on vectors 
  
  err = background_free_input(pba);

  return err;
}

/**
 * Free pointers inside background structure which were
 * allocated in input_read_parameters()
 *
 * @param pba Input: pointer to background structure
 * @return the error status
 */

int background_free_input(
                          struct background *pba
                          ) {

  int k;
  if (pba->Omega0_ncdm_tot != 0.){
    for(k=0; k<pba->N_ncdm; k++){
      free(pba->q_ncdm[k]);
      free(pba->w_ncdm[k]);
      free(pba->q_ncdm_bg[k]);
      free(pba->w_ncdm_bg[k]);
      free(pba->dlnf0_dlnq_ncdm[k]);
    }
    free(pba->ncdm_quadrature_strategy);
    free(pba->ncdm_input_q_size);
    free(pba->ncdm_qmax);
    free(pba->q_ncdm);
    free(pba->w_ncdm);
    free(pba->q_ncdm_bg);
    free(pba->w_ncdm_bg);
    free(pba->dlnf0_dlnq_ncdm);
    free(pba->q_size_ncdm);
    free(pba->q_size_ncdm_bg);
    free(pba->M_ncdm);
    free(pba->T_ncdm);
    free(pba->ksi_ncdm);
    free(pba->deg_ncdm);
    free(pba->Omega0_ncdm);
    free(pba->m_ncdm_in_eV);
    free(pba->factor_ncdm);
    if(pba->got_files!=NULL)
      free(pba->got_files);
    if(pba->ncdm_psd_files!=NULL)
      free(pba->ncdm_psd_files);
    if(pba->ncdm_psd_parameters!=NULL)
      free(pba->ncdm_psd_parameters);
  }

  if (pba->Omega0_scf != 0.){
    if (pba->scf_parameters != NULL)
      free(pba->scf_parameters);
  }

  return _SUCCESS_;
}

/**
 * Assign value to each relevant index in vectors of background quantities.
 *
 * @param pba Input: pointer to background structure
 * @return the error status
 */

int background_indices(
                       struct background *pba
                       ) {

  /** Summary: */

  /** - define local variables */

  /* a running index for the vector of background quantities */
  int index_bg;
  /* a running index for the vector of background quantities to be integrated */
  int index_bi;

  /** - initialize all flags: which species are present? */

  pba->has_cdm = _FALSE_;
  /* TK added GDM here */
  pba->has_gdm = _FALSE_;
  pba->has_ncdm = _FALSE_;
  pba->has_dcdm = _FALSE_;
  pba->has_dr = _FALSE_;
  pba->has_scf = _FALSE_;
  pba->has_lambda = _FALSE_;
  pba->has_fld = _FALSE_;
  pba->has_ur = _FALSE_;
  pba->has_curvature = _FALSE_;

  if (pba->Omega0_cdm != 0.){
    pba->has_cdm = _TRUE_;
  }

  /* TK added GDM here */
  if (pba->Omega0_gdm != 0.)
    pba->has_gdm = _TRUE_;

  if (pba->Omega0_ncdm_tot != 0.)
    pba->has_ncdm = _TRUE_;

  if (pba->Omega0_dcdmdr != 0.){
    pba->has_dcdm = _TRUE_;
    if (pba->Gamma_dcdm != 0.)
      pba->has_dr = _TRUE_;
  }

  if (pba->Omega0_scf != 0.)
    pba->has_scf = _TRUE_;

  if (pba->Omega0_lambda != 0.)
    pba->has_lambda = _TRUE_;

  if (pba->n_fld != 0){
    pba->has_fld = _TRUE_;
  }

  if (pba->Omega0_ur != 0.)
    pba->has_ur = _TRUE_;

  if (pba->sgnK != 0)
    pba->has_curvature = _TRUE_;


  /** - initialize all indices */

  index_bg=0;

  /* index for scale factor */
  class_define_index(pba->index_bg_a,_TRUE_,index_bg,1);

  /* - indices for H and its conformal-time-derivative */
  class_define_index(pba->index_bg_H,_TRUE_,index_bg,1);
  class_define_index(pba->index_bg_H_prime,_TRUE_,index_bg,1);

  /* - end of indices in the short vector of background values */
  pba->bg_size_short = index_bg;

  /* - index for rho_g (photon density) */
  class_define_index(pba->index_bg_rho_g,_TRUE_,index_bg,1);

  /* - index for rho_b (baryon density) */
  class_define_index(pba->index_bg_rho_b,_TRUE_,index_bg,1);

  /* - index for rho_cdm */
  class_define_index(pba->index_bg_rho_cdm,pba->has_cdm,index_bg,1);

  /* TK added this to initialise index for rho_gdm */
  /* - index for rho_gdm */
  class_define_index(pba->index_bg_rho_gdm,pba->has_gdm,index_bg,1);
  class_define_index(pba->index_bg_w_gdm,pba->has_gdm,index_bg,1);
  class_define_index(pba->index_bg_dw_gdm,pba->has_gdm,index_bg,1);


  /* - indices for ncdm. We only define the indices for ncdm1
     (density, pressure, pseudo-pressure), the other ncdm indices
     are contiguous */
  class_define_index(pba->index_bg_rho_ncdm1,pba->has_ncdm,index_bg,pba->N_ncdm);
  class_define_index(pba->index_bg_p_ncdm1,pba->has_ncdm,index_bg,pba->N_ncdm);
  class_define_index(pba->index_bg_pseudo_p_ncdm1,pba->has_ncdm,index_bg,pba->N_ncdm);

  /* - index for dcdm */
  class_define_index(pba->index_bg_rho_dcdm,pba->has_dcdm,index_bg,1);

  /* - index for dr */
  class_define_index(pba->index_bg_rho_dr,pba->has_dr,index_bg,1);

  /* - indices for scalar field */
  class_define_index(pba->index_bg_phi_scf,pba->has_scf,index_bg,1);
  class_define_index(pba->index_bg_phi_prime_scf,pba->has_scf,index_bg,1);
  class_define_index(pba->index_bg_V_scf,pba->has_scf,index_bg,1);
  class_define_index(pba->index_bg_dV_scf,pba->has_scf,index_bg,1);
  class_define_index(pba->index_bg_ddV_scf,pba->has_scf,index_bg,1);
  class_define_index(pba->index_bg_rho_scf,pba->has_scf,index_bg,1);
  class_define_index(pba->index_bg_p_scf,pba->has_scf,index_bg,1);

  /* - index for Lambda */
  class_define_index(pba->index_bg_rho_lambda,pba->has_lambda,index_bg,1);

  /* - index for fluid */
  class_define_index(pba->index_bg_rho_fld,pba->has_fld,index_bg,pba->n_fld);
  class_define_index(pba->index_bg_w_fld,pba->has_fld,index_bg,pba->n_fld);
  class_define_index(pba->index_bg_dw_fld,pba->has_fld,index_bg,pba->n_fld);

  /* - index for ultra-relativistic neutrinos/species */
  class_define_index(pba->index_bg_rho_ur,pba->has_ur,index_bg,1);

  /* - index for Omega_r (relativistic density fraction) */
  class_define_index(pba->index_bg_Omega_r,_TRUE_,index_bg,1);

  /* - put here additional ingredients that you want to appear in the
     normal vector */
  /*    */
  /*    */

  /* - end of indices in the normal vector of background values */
  pba->bg_size_normal = index_bg;

  /* - indices in the long version : */

  /* -> critical density */
  class_define_index(pba->index_bg_rho_crit,_TRUE_,index_bg,1);

  /* - index for Omega_m (non-relativistic density fraction) */
  class_define_index(pba->index_bg_Omega_m,_TRUE_,index_bg,1);

  /* -> conformal distance */
  class_define_index(pba->index_bg_conf_distance,_TRUE_,index_bg,1);

  /* -> angular diameter distance */
  class_define_index(pba->index_bg_ang_distance,_TRUE_,index_bg,1);

  /* -> luminosity distance */
  class_define_index(pba->index_bg_lum_distance,_TRUE_,index_bg,1);

  /* -> proper time (for age of the Universe) */
  class_define_index(pba->index_bg_time,_TRUE_,index_bg,1);

  /* -> conformal sound horizon */
  class_define_index(pba->index_bg_rs,_TRUE_,index_bg,1);

  /* -> density growth factor in dust universe */
  class_define_index(pba->index_bg_D,_TRUE_,index_bg,1);

  /* -> velocity growth factor in dust universe */
  class_define_index(pba->index_bg_f,_TRUE_,index_bg,1);

  /* -> put here additional quantities describing background */
  /*    */
  /*    */

  /* -> end of indices in the long vector of background values */
  pba->bg_size = index_bg;

  /* - now, indices in vector of variables to integrate.
     First {B} variables, then {C} variables. */

  index_bi=0;

  /* -> scale factor */
  class_define_index(pba->index_bi_a,_TRUE_,index_bi,1);

  /* -> energy density in DCDM */
  class_define_index(pba->index_bi_rho_dcdm,pba->has_dcdm,index_bi,1);

  /* -> energy density in DR */
  class_define_index(pba->index_bi_rho_dr,pba->has_dr,index_bi,1);

  /* -> energy density in fluid */
  class_define_index(pba->index_bi_rho_fld,pba->has_fld,index_bi,pba->n_fld);
  // TK added GDM here 
  /* -> energy density in GDM */
  class_define_index(pba->index_bi_rho_gdm,pba->has_gdm,index_bi,1);

  /* -> scalar field and its derivative wrt conformal time (Zuma) */
  class_define_index(pba->index_bi_phi_scf,pba->has_scf,index_bi,1);
  class_define_index(pba->index_bi_phi_prime_scf,pba->has_scf,index_bi,1);

  /* End of {B} variables, now continue with {C} variables */
  pba->bi_B_size = index_bi;

  /* -> proper time (for age of the Universe) */
  class_define_index(pba->index_bi_time,_TRUE_,index_bi,1);

  /* -> sound horizon */
  class_define_index(pba->index_bi_rs,_TRUE_,index_bi,1);

  /* -> Second order equation for growth factor */
  class_define_index(pba->index_bi_D,_TRUE_,index_bi,1);
  class_define_index(pba->index_bi_D_prime,_TRUE_,index_bi,1);

  /* -> index for conformal time in vector of variables to integrate */
  class_define_index(pba->index_bi_tau,_TRUE_,index_bi,1);

  /* -> end of indices in the vector of variables to integrate */
  pba->bi_size = index_bi;

  /* index_bi_tau must be the last index, because tau is part of this vector for the purpose of being stored, */
  /* but it is not a quantity to be integrated (since integration is over tau itself) */
  class_test(pba->index_bi_tau != index_bi-1,
             pba->error_message,
             "background integration requires index_bi_tau to be the last of all index_bi's");

  /* flags for calling the interpolation routine */

  pba->short_info=0;
  pba->normal_info=1;
  pba->long_info=2;

  pba->inter_normal=0;
  pba->inter_closeby=1;

  return _SUCCESS_;

}

/**
 * This is the routine where the distribution function f0(q) of each
 * ncdm species is specified (it is the only place to modify if you
 * need a partlar f0(q))
 *
 * @param pbadist Input:  structure containing all parameters defining f0(q)
 * @param q       Input:  momentum
 * @param f0      Output: phase-space distribution
 */

int background_ncdm_distribution(
                                 void * pbadist,
                                 double q,
                                 double * f0
                                 ) {
  struct background * pba;
  struct background_parameters_for_distributions * pbadist_local;
  int n_ncdm,lastidx;
  double ksi;
  double qlast,dqlast,f0last,df0last;
  double *param;
  /* Variables corresponding to entries in param: */
  //double square_s12,square_s23,square_s13;
  //double mixing_matrix[3][3];
  //int i;

  /** - extract from the input structure pbadist all the relevant information */
  pbadist_local = pbadist;          /* restore actual format of pbadist */
  pba = pbadist_local->pba;         /* extract the background structure from it */
  param = pba->ncdm_psd_parameters; /* extract the optional parameter list from it */
  n_ncdm = pbadist_local->n_ncdm;   /* extract index of ncdm species under consideration */
  ksi = pba->ksi_ncdm[n_ncdm];      /* extract chemical potential */

  /** - shall we interpolate in file, or shall we use analytical formula below? */

  /** - a) deal first with the case of interpolating in files */
  if (pba->got_files[n_ncdm]==_TRUE_) {

    lastidx = pbadist_local->tablesize-1;
    if(q<pbadist_local->q[0]){
      //Handle q->0 case:
      *f0 = pbadist_local->f0[0];
    }
    else if(q>pbadist_local->q[lastidx]){
      //Handle q>qmax case (ensure continuous and derivable function with Boltzmann tail):
      qlast=pbadist_local->q[lastidx];
      f0last=pbadist_local->f0[lastidx];
      dqlast=qlast - pbadist_local->q[lastidx-1];
      df0last=f0last - pbadist_local->f0[lastidx-1];

      *f0 = f0last*exp(-(qlast-q)*df0last/f0last/dqlast);
    }
    else{
      //Do interpolation:
      class_call(array_interpolate_spline(
                                          pbadist_local->q,
                                          pbadist_local->tablesize,
                                          pbadist_local->f0,
                                          pbadist_local->d2f0,
                                          1,
                                          q,
                                          &pbadist_local->last_index,
                                          f0,
                                          1,
                                          pba->error_message),
                 pba->error_message,     pba->error_message);
    }
  }

  /** - b) deal now with case of reading analytical function */
  else{
    /**
       Next enter your analytic expression(s) for the p.s.d.'s. If
       you need different p.s.d.'s for different species, put each
       p.s.d inside a condition, like for instance: if (n_ncdm==2)
       {*f0=...}.  Remember that n_ncdm = 0 refers to the first
       species.
    */

    /**************************************************/
    /*    FERMI-DIRAC INCLUDING CHEMICAL POTENTIALS   */
    /**************************************************/

    *f0 = 1.0/pow(2*_PI_,3)*(1./(exp(q-ksi)+1.) +1./(exp(q+ksi)+1.));

    /**************************************************/

    /** This form is only appropriate for approximate studies, since in
        reality the chemical potentials are associated with flavor
        eigenstates, not mass eigenstates. It is easy to take this into
        account by introducing the mixing angles. In the later part
        (not read by the code) we illustrate how to do this. */

    if (_FALSE_) {

      /* We must use the list of extra parameters read in input, stored in the
         ncdm_psd_parameter list, extracted above from the structure
         and now called param[..] */

      /* check that this list has been read */
      class_test(param == NULL,
                 pba->error_message,
                 "Analytic expression wants to use 'ncdm_psd_parameters', but they have not been entered!");

      /* extract values from the list (in this example, mixing angles) */
      double square_s12=param[0];
      double square_s23=param[1];
      double square_s13=param[2];

      /* infer mixing matrix */
      double mixing_matrix[3][3];
      int i;

      mixing_matrix[0][0]=pow(fabs(sqrt((1-square_s12)*(1-square_s13))),2);
      mixing_matrix[0][1]=pow(fabs(sqrt(square_s12*(1-square_s13))),2);
      mixing_matrix[0][2]=fabs(square_s13);
      mixing_matrix[1][0]=pow(fabs(sqrt((1-square_s12)*square_s13*square_s23)+sqrt(square_s12*(1-square_s23))),2);
      mixing_matrix[1][1]=pow(fabs(sqrt(square_s12*square_s23*square_s13)-sqrt((1-square_s12)*(1-square_s23))),2);
      mixing_matrix[1][2]=pow(fabs(sqrt(square_s23*(1-square_s13))),2);
      mixing_matrix[2][0]=pow(fabs(sqrt(square_s12*square_s23)-sqrt((1-square_s12)*square_s13*(1-square_s23))),2);
      mixing_matrix[2][1]=pow(sqrt((1-square_s12)*square_s23)+sqrt(square_s12*square_s13*(1-square_s23)),2);
      mixing_matrix[2][2]=pow(fabs(sqrt((1-square_s13)*(1-square_s23))),2);

      /* loop over flavor eigenstates and compute psd of mass eigenstates */
      *f0=0.0;
      for(i=0;i<3;i++){

    	*f0 += mixing_matrix[i][n_ncdm]*1.0/pow(2*_PI_,3)*(1./(exp(q-pba->ksi_ncdm[i])+1.) +1./(exp(q+pba->ksi_ncdm[i])+1.));

      }
    } /* end of region not used, but shown as an example */
  }

  return _SUCCESS_;
}

/**
 * This function is only used for the purpose of finding optimal
 * quadrature weights. The logic is: if we can accurately convolve
 * f0(q) with this function, then we can convolve it accurately with
 * any other relevant function.
 *
 * @param pbadist Input:  structure containing all background parameters
 * @param q       Input:  momentum
 * @param test    Output: value of the test function test(q)
 */

int background_ncdm_test_function(
                                  void * pbadist,
                                  double q,
                                  double * test
                                  ) {

  double c = 2.0/(3.0*_zeta3_);
  double d = 120.0/(7.0*pow(_PI_,4));
  double e = 2.0/(45.0*_zeta5_);

  /** Using a + bq creates problems for otherwise acceptable distributions
      which diverges as \f$ 1/r \f$ or \f$ 1/r^2 \f$ for \f$ r\to 0 \f$*/
  *test = pow(2.0*_PI_,3)/6.0*(c*q*q-d*q*q*q-e*q*q*q*q);

  return _SUCCESS_;
}

/**
 * This function finds optimal quadrature weights for each ncdm
 * species
 *
 * @param ppr Input: precision structure
 * @param pba Input/Output: background structure
 */

int background_ncdm_init(
                         struct precision *ppr,
                         struct background *pba
                         ) {

  int index_q, k,tolexp,row,status,filenum;
  double f0m2,f0m1,f0,f0p1,f0p2,dq,q,df0dq,tmp1,tmp2;
  struct background_parameters_for_distributions pbadist;
  FILE *psdfile;

  pbadist.pba = pba;

  /* Allocate pointer arrays: */
  class_alloc(pba->q_ncdm, sizeof(double*)*pba->N_ncdm,pba->error_message);
  class_alloc(pba->w_ncdm, sizeof(double*)*pba->N_ncdm,pba->error_message);
  class_alloc(pba->q_ncdm_bg, sizeof(double*)*pba->N_ncdm,pba->error_message);
  class_alloc(pba->w_ncdm_bg, sizeof(double*)*pba->N_ncdm,pba->error_message);
  class_alloc(pba->dlnf0_dlnq_ncdm, sizeof(double*)*pba->N_ncdm,pba->error_message);

  /* Allocate pointers: */
  class_alloc(pba->q_size_ncdm,sizeof(int)*pba->N_ncdm,pba->error_message);
  class_alloc(pba->q_size_ncdm_bg,sizeof(int)*pba->N_ncdm,pba->error_message);
  class_alloc(pba->factor_ncdm,sizeof(double)*pba->N_ncdm,pba->error_message);

  for(k=0, filenum=0; k<pba->N_ncdm; k++){
    pbadist.n_ncdm = k;
    pbadist.q = NULL;
    pbadist.tablesize = 0;
    /*Do we need to read in a file to interpolate the distribution function? */
    if ((pba->got_files!=NULL)&&(pba->got_files[k]==_TRUE_)){
      psdfile = fopen(pba->ncdm_psd_files+filenum*_ARGUMENT_LENGTH_MAX_,"r");
      class_test(psdfile == NULL,pba->error_message,
                 "Could not open file %s!",pba->ncdm_psd_files+filenum*_ARGUMENT_LENGTH_MAX_);
      // Find size of table:
      for (row=0,status=2; status==2; row++){
        status = fscanf(psdfile,"%lf %lf",&tmp1,&tmp2);
      }
      rewind(psdfile);
      pbadist.tablesize = row-1;

      /*Allocate room for interpolation table: */
      class_alloc(pbadist.q,sizeof(double)*pbadist.tablesize,pba->error_message);
      class_alloc(pbadist.f0,sizeof(double)*pbadist.tablesize,pba->error_message);
      class_alloc(pbadist.d2f0,sizeof(double)*pbadist.tablesize,pba->error_message);
      for (row=0; row<pbadist.tablesize; row++){
        status = fscanf(psdfile,"%lf %lf",
                        &pbadist.q[row],&pbadist.f0[row]);
        //		printf("(q,f0) = (%g,%g)\n",pbadist.q[row],pbadist.f0[row]);
      }
      fclose(psdfile);
      /* Call spline interpolation: */
      class_call(array_spline_table_lines(pbadist.q,
                                          pbadist.tablesize,
                                          pbadist.f0,
                                          1,
                                          pbadist.d2f0,
                                          _SPLINE_EST_DERIV_,
                                          pba->error_message),
                 pba->error_message,
                 pba->error_message);
      filenum++;
    }

    /* Handle perturbation qsampling: */
    if (pba->ncdm_quadrature_strategy[k]==qm_auto){
      /** Automatic q-sampling for this species */
      class_alloc(pba->q_ncdm[k],_QUADRATURE_MAX_*sizeof(double),pba->error_message);
      class_alloc(pba->w_ncdm[k],_QUADRATURE_MAX_*sizeof(double),pba->error_message);

      class_call(get_qsampling(pba->q_ncdm[k],
			       pba->w_ncdm[k],
			       &(pba->q_size_ncdm[k]),
			       _QUADRATURE_MAX_,
			       ppr->tol_ncdm,
			       pbadist.q,
			       pbadist.tablesize,
			       background_ncdm_test_function,
			       background_ncdm_distribution,
			       &pbadist,
			       pba->error_message),
		 pba->error_message,
		 pba->error_message);
      pba->q_ncdm[k]=realloc(pba->q_ncdm[k],pba->q_size_ncdm[k]*sizeof(double));
      pba->w_ncdm[k]=realloc(pba->w_ncdm[k],pba->q_size_ncdm[k]*sizeof(double));


      if (pba->background_verbose > 0)
	printf("ncdm species i=%d sampled with %d points for purpose of perturbation integration\n",
	       k+1,
	       pba->q_size_ncdm[k]);

      /* Handle background q_sampling: */
      class_alloc(pba->q_ncdm_bg[k],_QUADRATURE_MAX_BG_*sizeof(double),pba->error_message);
      class_alloc(pba->w_ncdm_bg[k],_QUADRATURE_MAX_BG_*sizeof(double),pba->error_message);

      class_call(get_qsampling(pba->q_ncdm_bg[k],
			       pba->w_ncdm_bg[k],
			       &(pba->q_size_ncdm_bg[k]),
			       _QUADRATURE_MAX_BG_,
			       ppr->tol_ncdm_bg,
			       pbadist.q,
			       pbadist.tablesize,
			       background_ncdm_test_function,
			       background_ncdm_distribution,
			       &pbadist,
			       pba->error_message),
		 pba->error_message,
		 pba->error_message);


      pba->q_ncdm_bg[k]=realloc(pba->q_ncdm_bg[k],pba->q_size_ncdm_bg[k]*sizeof(double));
      pba->w_ncdm_bg[k]=realloc(pba->w_ncdm_bg[k],pba->q_size_ncdm_bg[k]*sizeof(double));

      /** - in verbose mode, inform user of number of sampled momenta
	  for background quantities */
      if (pba->background_verbose > 0)
	printf("ncdm species i=%d sampled with %d points for purpose of background integration\n",
	       k+1,
	       pba->q_size_ncdm_bg[k]);
    }
    else{
      /** Manual q-sampling for this species. Same sampling used for both perturbation and background sampling, since this will usually be a high precision setting anyway */
      pba->q_size_ncdm_bg[k] = pba->ncdm_input_q_size[k];
      pba->q_size_ncdm[k] = pba->ncdm_input_q_size[k];
      class_alloc(pba->q_ncdm_bg[k],pba->q_size_ncdm_bg[k]*sizeof(double),pba->error_message);
      class_alloc(pba->w_ncdm_bg[k],pba->q_size_ncdm_bg[k]*sizeof(double),pba->error_message);
      class_alloc(pba->q_ncdm[k],pba->q_size_ncdm[k]*sizeof(double),pba->error_message);
      class_alloc(pba->w_ncdm[k],pba->q_size_ncdm[k]*sizeof(double),pba->error_message);
      class_call(get_qsampling_manual(pba->q_ncdm[k],
				      pba->w_ncdm[k],
				      pba->q_size_ncdm[k],
				      pba->ncdm_qmax[k],
				      pba->ncdm_quadrature_strategy[k],
				      pbadist.q,
				      pbadist.tablesize,
				      background_ncdm_distribution,
				      &pbadist,
				      pba->error_message),
		 pba->error_message,
		 pba->error_message);
      for (index_q=0; index_q<pba->q_size_ncdm[k]; index_q++) {
	pba->q_ncdm_bg[k] = pba->q_ncdm[k];
	pba->w_ncdm_bg[k] = pba->w_ncdm[k];
      }
    /** - in verbose mode, inform user of number of sampled momenta
        for background quantities */
      if (pba->background_verbose > 0)
	printf("ncdm species i=%d sampled with %d points for purpose of background andperturbation integration using the manual method\n",
	       k+1,
	       pba->q_size_ncdm[k]);
    }

    class_alloc(pba->dlnf0_dlnq_ncdm[k],
                pba->q_size_ncdm[k]*sizeof(double),
                pba->error_message);


    for (index_q=0; index_q<pba->q_size_ncdm[k]; index_q++) {
      q = pba->q_ncdm[k][index_q];
      class_call(background_ncdm_distribution(&pbadist,q,&f0),
                 pba->error_message,pba->error_message);

      //Loop to find appropriate dq:
      for(tolexp=_PSD_DERIVATIVE_EXP_MIN_; tolexp<_PSD_DERIVATIVE_EXP_MAX_; tolexp++){

        if (index_q == 0){
          dq = MIN((0.5-ppr->smallest_allowed_variation)*q,2*exp(tolexp)*(pba->q_ncdm[k][index_q+1]-q));
        }
        else if (index_q == pba->q_size_ncdm[k]-1){
          dq = exp(tolexp)*2.0*(pba->q_ncdm[k][index_q]-pba->q_ncdm[k][index_q-1]);
        }
        else{
          dq = exp(tolexp)*(pba->q_ncdm[k][index_q+1]-pba->q_ncdm[k][index_q-1]);
        }

        class_call(background_ncdm_distribution(&pbadist,q-2*dq,&f0m2),
                   pba->error_message,pba->error_message);
        class_call(background_ncdm_distribution(&pbadist,q+2*dq,&f0p2),
                   pba->error_message,pba->error_message);

        if (fabs((f0p2-f0m2)/f0)>sqrt(ppr->smallest_allowed_variation)) break;
      }

      class_call(background_ncdm_distribution(&pbadist,q-dq,&f0m1),
                 pba->error_message,pba->error_message);
      class_call(background_ncdm_distribution(&pbadist,q+dq,&f0p1),
                 pba->error_message,pba->error_message);
      //5 point estimate of the derivative:
      df0dq = (+f0m2-8*f0m1+8*f0p1-f0p2)/12.0/dq;
      //printf("df0dq[%g] = %g. dlf=%g ?= %g. f0 =%g.\n",q,df0dq,q/f0*df0dq,
      //Avoid underflow in extreme tail:
      if (fabs(f0)==0.)
        pba->dlnf0_dlnq_ncdm[k][index_q] = -q; /* valid for whatever f0 with exponential tail in exp(-q) */
      else
        pba->dlnf0_dlnq_ncdm[k][index_q] = q/f0*df0dq;
    }

    pba->factor_ncdm[k]=pba->deg_ncdm[k]*4*_PI_*pow(pba->T_cmb*pba->T_ncdm[k]*_k_B_,4)*8*_PI_*_G_
      /3./pow(_h_P_/2./_PI_,3)/pow(_c_,7)*_Mpc_over_m_*_Mpc_over_m_;

    /* If allocated, deallocate interpolation table:  */
    if ((pba->got_files!=NULL)&&(pba->got_files[k]==_TRUE_)){
      free(pbadist.q);
      free(pbadist.f0);
      free(pbadist.d2f0);
    }
  }


  return _SUCCESS_;
}

/**
 * For a given ncdm species: given the quadrature weights, the mass
 * and the redshift, find background quantities by a quick weighted
 * sum over.  Input parameters passed as NULL pointers are not
 * evaluated for speed-up
 *
 * @param qvec     Input: sampled momenta
 * @param wvec     Input: quadrature weights
 * @param qsize    Input: number of momenta/weights
 * @param M        Input: mass
 * @param factor   Input: normalization factor for the p.s.d.
 * @param z        Input: redshift
 * @param n        Output: number density
 * @param rho      Output: energy density
 * @param p        Output: pressure
 * @param drho_dM  Output: derivative used in next function
 * @param pseudo_p Output: pseudo-pressure used in perturbation module for fluid approx
 *
 */

int background_ncdm_momenta(
                            /* Only calculate for non-NULL pointers: */
                            double * qvec,
                            double * wvec,
                            int qsize,
                            double M,
                            double factor,
                            double z,
                            double * n,
                            double * rho, // density
                            double * p,   // pressure
                            double * drho_dM,  // d rho / d M used in next function
                            double * pseudo_p  // pseudo-p used in ncdm fluid approx
                            ) {

  int index_q;
  double epsilon;
  double q2;
  double factor2;
  /** Summary: */

  /** - rescale normalization at given redshift */
  factor2 = factor*pow(1+z,4);

  /** - initialize quantities */
  if (n!=NULL) *n = 0.;
  if (rho!=NULL) *rho = 0.;
  if (p!=NULL) *p = 0.;
  if (drho_dM!=NULL) *drho_dM = 0.;
  if (pseudo_p!=NULL) *pseudo_p = 0.;

  /** - loop over momenta */
  for (index_q=0; index_q<qsize; index_q++) {

    /* squared momentum */
    q2 = qvec[index_q]*qvec[index_q];

    /* energy */
    epsilon = sqrt(q2+M*M/(1.+z)/(1.+z));

    /* integrand of the various quantities */
    if (n!=NULL) *n += q2*wvec[index_q];
    if (rho!=NULL) *rho += q2*epsilon*wvec[index_q];
    if (p!=NULL) *p += q2*q2/3./epsilon*wvec[index_q];
    if (drho_dM!=NULL) *drho_dM += q2*M/(1.+z)/(1.+z)/epsilon*wvec[index_q];
    if (pseudo_p!=NULL) *pseudo_p += pow(q2/epsilon,3)/3.0*wvec[index_q];
  }

  /** - adjust normalization */
  if (n!=NULL) *n *= factor2*(1.+z);
  if (rho!=NULL) *rho *= factor2;
  if (p!=NULL) *p *= factor2;
  if (drho_dM!=NULL) *drho_dM *= factor2;
  if (pseudo_p!=NULL) *pseudo_p *=factor2;

  return _SUCCESS_;
}

/**
 * When the user passed the density fraction Omega_ncdm or
 * omega_ncdm in input but not the mass, infer the mass with Newton iteration method.
 *
 * @param ppr    Input: precision structure
 * @param pba    Input/Output: background structure
 * @param n_ncdm Input: index of ncdm species
 */

int background_ncdm_M_from_Omega(
                                 struct precision *ppr,
                                 struct background *pba,
                                 int n_ncdm
                                 ) {
  double rho0,rho,n,M,deltaM,drhodM;
  int iter,maxiter=50;

  rho0 = pba->H0*pba->H0*pba->Omega0_ncdm[n_ncdm]; /*Remember that rho is defined such that H^2=sum(rho_i) */
  M = 0.0;

  background_ncdm_momenta(pba->q_ncdm_bg[n_ncdm],
                          pba->w_ncdm_bg[n_ncdm],
                          pba->q_size_ncdm_bg[n_ncdm],
                          M,
                          pba->factor_ncdm[n_ncdm],
                          0.,
                          &n,
                          &rho,
                          NULL,
                          NULL,
                          NULL);

  /* Is the value of Omega less than a massless species?*/
  class_test(rho0<rho,pba->error_message,
             "The value of Omega for the %dth species, %g, is less than for a massless species! It should be atleast %g. Check your input.",
             n_ncdm,pba->Omega0_ncdm[n_ncdm],pba->Omega0_ncdm[n_ncdm]*rho/rho0);

  /* In the strict NR limit we have rho = n*(M) today, giving a zeroth order guess: */
  M = rho0/n; /* This is our guess for M. */
  for (iter=1; iter<=maxiter; iter++){

    /* Newton iteration. First get relevant quantities at M: */
    background_ncdm_momenta(pba->q_ncdm_bg[n_ncdm],
                            pba->w_ncdm_bg[n_ncdm],
                            pba->q_size_ncdm_bg[n_ncdm],
                            M,
                            pba->factor_ncdm[n_ncdm],
                            0.,
                            NULL,
                            &rho,
                            NULL,
                            &drhodM,
                            NULL);

    deltaM = (rho0-rho)/drhodM; /* By definition of the derivative */
    if ((M+deltaM)<0.0) deltaM = -M/2.0; /* Avoid overshooting to negative M value. */
    M += deltaM; /* Update value of M.. */
    if (fabs(deltaM/M)<ppr->tol_M_ncdm){
      /* Accuracy reached.. */
      pba->M_ncdm[n_ncdm] = M;
      break;
    }
  }
  class_test(iter>=maxiter,pba->error_message,
             "Newton iteration could not converge on a mass for some reason.");
  return _SUCCESS_;
}

/**
 *  This function integrates the background over time, allocates and
 *  fills the background table
 *
 * @param ppr Input: precision structure
 * @param pba Input/Output: background structure
 */

int background_solve(
                     struct precision *ppr,
                     struct background *pba
                     ) {

  /** Summary: */

  /** - define local variables */

  /* contains all quantities relevant for the integration algorithm */
  struct generic_integrator_workspace gi;
  /* parameters and workspace for the background_derivs function */
  struct background_parameters_and_workspace bpaw;
  /* a growing table (since the number of time steps is not known a priori) */
  growTable gTable;
  /* needed for growing table */
  double * pData;
  /* needed for growing table */
  void * memcopy_result;
  /* initial conformal time */
  double tau_start;
  /* final conformal time */
  double tau_end;
  /* an index running over bi indices */
  int i;
  /* vector of quantities to be integrated */
  double * pvecback_integration;
  /* vector of all background quantities */
  double * pvecback;
  /* necessary for calling array_interpolate(), but never used */
  int last_index=0;
  /* comoving radius coordinate in Mpc (equal to conformal distance in flat case) */
  double comoving_radius=0.;
  /* parameters required to get m_fld when playing with axion */
  double tau_of_ac;
  int n;

  bpaw.pba = pba;
  class_alloc(pvecback,pba->bg_size*sizeof(double),pba->error_message);
  bpaw.pvecback = pvecback;

  /** - allocate vector of quantities to be integrated */
  class_alloc(pvecback_integration,pba->bi_size*sizeof(double),pba->error_message);

  /** - initialize generic integrator with initialize_generic_integrator() */

  /* Size of vector to integrate is (pba->bi_size-1) rather than
   * (pba->bi_size), since tau is not integrated.
   */
  class_call(initialize_generic_integrator((pba->bi_size-1),&gi),
             gi.error_message,
             pba->error_message);

  /** - impose initial conditions with background_initial_conditions() */
  class_call(background_initial_conditions(ppr,pba,pvecback,pvecback_integration),
             pba->error_message,
             pba->error_message);

  /* here tau_end is in fact the initial time (in the next loop
     tau_start = tau_end) */
  tau_end=pvecback_integration[pba->index_bi_tau];

  /** - create a growTable with gt_init() */
  class_call(gt_init(&gTable),
             gTable.error_message,
             pba->error_message);

  /* initialize the counter for the number of steps */
  pba->bt_size=0;

  /** - loop over integration steps: call background_functions(), find step size, save data in growTable with gt_add(), perform one step with generic_integrator(), store new value of tau */

  while (pvecback_integration[pba->index_bi_a] < pba->a_today) {

    tau_start = tau_end;

    /* -> find step size (trying to adjust the last step as close as possible to the one needed to reach a=a_today; need not be exact, difference corrected later) */
    class_call(background_functions(pba,pvecback_integration, pba->short_info, pvecback),
               pba->error_message,
               pba->error_message);

    if ((pvecback_integration[pba->index_bi_a]*(1.+ppr->back_integration_stepsize)) < pba->a_today) {
      tau_end = tau_start + ppr->back_integration_stepsize / (pvecback_integration[pba->index_bi_a]*pvecback[pba->index_bg_H]);
      /* no possible segmentation fault here: non-zeroness of "a" has been checked in background_functions() */
    }
    else {
      tau_end = tau_start + (pba->a_today/pvecback_integration[pba->index_bi_a]-1.) / (pvecback_integration[pba->index_bi_a]*pvecback[pba->index_bg_H]);
      /* no possible segmentation fault here: non-zeroness of "a" has been checked in background_functions() */
    }

    class_test((tau_end-tau_start)/tau_start < ppr->smallest_allowed_variation,
               pba->error_message,
               "integration step: relative change in time =%e < machine precision : leads either to numerical error or infinite loop",(tau_end-tau_start)/tau_start);

    /* -> save data in growTable */
    class_call(gt_add(&gTable,_GT_END_,(void *) pvecback_integration,sizeof(double)*pba->bi_size),
               gTable.error_message,
               pba->error_message);
    pba->bt_size++;

    /* -> perform one step */
    class_call(generic_integrator(background_derivs,
                                  tau_start,
                                  tau_end,
                                  pvecback_integration,
                                  &bpaw,
                                  ppr->tol_background_integration,
                                  ppr->smallest_allowed_variation,
                                  &gi),
               gi.error_message,
               pba->error_message);

    /* -> store value of tau */
    pvecback_integration[pba->index_bi_tau]=tau_end;

  }

  /** - save last data in growTable with gt_add() */
  class_call(gt_add(&gTable,_GT_END_,(void *) pvecback_integration,sizeof(double)*pba->bi_size),
             gTable.error_message,
             pba->error_message);
  pba->bt_size++;


  /* integration finished */

  /** - clean up generic integrator with cleanup_generic_integrator() */
  class_call(cleanup_generic_integrator(&gi),
             gi.error_message,
             pba->error_message);

  /** - retrieve data stored in the growTable with gt_getPtr() */
  class_call(gt_getPtr(&gTable,(void**)&pData),
             gTable.error_message,
             pba->error_message);

  /** - interpolate to get quantities precisely today with array_interpolate() */
  class_call(array_interpolate(
                               pData,
                               pba->bi_size,
                               pba->bt_size,
                               pba->index_bi_a,
                               pba->a_today,
                               &last_index,
                               pvecback_integration,
                               pba->bi_size,
                               pba->error_message),
             pba->error_message,
             pba->error_message);

  /* substitute last line with quantities today */
  for (i=0; i<pba->bi_size; i++)
    pData[(pba->bt_size-1)*pba->bi_size+i]=pvecback_integration[i];

  /** - deduce age of the Universe */
  /* -> age in Gyears */
  pba->age = pvecback_integration[pba->index_bi_time]/_Gyr_over_Mpc_;
  /* -> conformal age in Mpc */
  pba->conformal_age = pvecback_integration[pba->index_bi_tau];
  /* -> contribution of decaying dark matter and dark radiation to the critical density today: */
  if (pba->has_dcdm == _TRUE_){
    pba->Omega0_dcdm = pvecback_integration[pba->index_bi_rho_dcdm]/pba->H0/pba->H0;
  }
  if (pba->has_dr == _TRUE_){
    pba->Omega0_dr = pvecback_integration[pba->index_bi_rho_dr]/pba->H0/pba->H0;
  }


  /** - allocate background tables */
  class_alloc(pba->tau_table,pba->bt_size * sizeof(double),pba->error_message);

  class_alloc(pba->z_table,pba->bt_size * sizeof(double),pba->error_message);

  class_alloc(pba->d2tau_dz2_table,pba->bt_size * sizeof(double),pba->error_message);

  class_alloc(pba->background_table,pba->bt_size * pba->bg_size * sizeof(double),pba->error_message);

  class_alloc(pba->d2background_dtau2_table,pba->bt_size * pba->bg_size * sizeof(double),pba->error_message);

  /** - In a loop over lines, fill background table using the result of the integration plus background_functions() */
  for (i=0; i < pba->bt_size; i++) {

    /* -> establish correspondence between the integrated variable and the bg variables */

    pba->tau_table[i] = pData[i*pba->bi_size+pba->index_bi_tau];

    class_test(pData[i*pba->bi_size+pba->index_bi_a] <= 0.,
               pba->error_message,
               "a = %e instead of strictly positiv",pData[i*pba->bi_size+pba->index_bi_a]);

    pba->z_table[i] = pba->a_today/pData[i*pba->bi_size+pba->index_bi_a]-1.;

    pvecback[pba->index_bg_time] = pData[i*pba->bi_size+pba->index_bi_time];
    pvecback[pba->index_bg_conf_distance] = pba->conformal_age - pData[i*pba->bi_size+pba->index_bi_tau];

    if (pba->sgnK == 0) comoving_radius = pvecback[pba->index_bg_conf_distance];
    else if (pba->sgnK == 1) comoving_radius = sin(sqrt(pba->K)*pvecback[pba->index_bg_conf_distance])/sqrt(pba->K);
    else if (pba->sgnK == -1) comoving_radius = sinh(sqrt(-pba->K)*pvecback[pba->index_bg_conf_distance])/sqrt(-pba->K);

    pvecback[pba->index_bg_ang_distance] = pba->a_today*comoving_radius/(1.+pba->z_table[i]);
    pvecback[pba->index_bg_lum_distance] = pba->a_today*comoving_radius*(1.+pba->z_table[i]);
    pvecback[pba->index_bg_rs] = pData[i*pba->bi_size+pba->index_bi_rs];

    /* -> compute all other quantities depending only on {B} variables.
       The value of {B} variables in pData are also copied to pvecback.*/
    class_call(background_functions(pba,pData+i*pba->bi_size, pba->long_info, pvecback),
               pba->error_message,
               pba->error_message);

    /* -> compute growth functions (valid in dust universe) */

    /* Normalise D(z=0)=1 and construct f = D_prime/(aHD) */
    pvecback[pba->index_bg_D] = pData[i*pba->bi_size+pba->index_bi_D]/pData[(pba->bt_size-1)*pba->bi_size+pba->index_bi_D];
    pvecback[pba->index_bg_f] = pData[i*pba->bi_size+pba->index_bi_D_prime]/
      (pData[i*pba->bi_size+pba->index_bi_D]*pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H]);

    /* -> write in the table */
    memcopy_result = memcpy(pba->background_table + i*pba->bg_size,pvecback,pba->bg_size*sizeof(double));

    class_test(memcopy_result != pba->background_table + i*pba->bg_size,
               pba->error_message,
               "cannot copy data back to pba->background_table");
  }

  /** - free the growTable with gt_free() */

  class_call(gt_free(&gTable),
             gTable.error_message,
             pba->error_message);

  /** - fill tables of second derivatives (in view of spline interpolation) */
  class_call(array_spline_table_lines(pba->z_table,
                                      pba->bt_size,
                                      pba->tau_table,
                                      1,
                                      pba->d2tau_dz2_table,
                                      _SPLINE_EST_DERIV_,
                                      pba->error_message),
             pba->error_message,
             pba->error_message);

  class_call(array_spline_table_lines(pba->tau_table,
                                      pba->bt_size,
                                      pba->background_table,
                                      pba->bg_size,
                                      pba->d2background_dtau2_table,
                                      _SPLINE_EST_DERIV_,
                                      pba->error_message),
             pba->error_message,
             pba->error_message);

  /** - compute remaining "related parameters"
   *     - so-called "effective neutrino number", computed at earliest
      time in interpolation table. This should be seen as a
      definition: Neff is the equivalent number of
      instantaneously-decoupled neutrinos accounting for the
      radiation density, beyond photons */
  pba->Neff = (pba->background_table[pba->index_bg_Omega_r]
               *pba->background_table[pba->index_bg_rho_crit]
               -pba->background_table[pba->index_bg_rho_g])
    /(7./8.*pow(4./11.,4./3.)*pba->background_table[pba->index_bg_rho_g]);



  /** - done */
  if (pba->background_verbose > 0) {
    printf(" -> age = %f Gyr\n",pba->age);
    printf(" -> conformal age = %f Mpc\n",pba->conformal_age);
  }

  if (pba->background_verbose > 2) {
    if ((pba->has_dcdm == _TRUE_)&&(pba->has_dr == _TRUE_)){
      printf("    Decaying Cold Dark Matter details: (DCDM --> DR)\n");
      printf("     -> Omega0_dcdm = %f\n",pba->Omega0_dcdm);
      printf("     -> Omega0_dr = %f\n",pba->Omega0_dr);
      printf("     -> Omega0_dr+Omega0_dcdm = %f, input value = %f\n",
             pba->Omega0_dr+pba->Omega0_dcdm,pba->Omega0_dcdmdr);
      printf("     -> Omega_ini_dcdm/Omega_b = %f\n",pba->Omega_ini_dcdm/pba->Omega0_b);
    }
    if (pba->has_scf == _TRUE_){
      printf("    Scalar field details:\n");
      printf("     -> Omega_scf = %g, wished %g\n",
             pvecback[pba->index_bg_rho_scf]/pvecback[pba->index_bg_rho_crit], pba->Omega0_scf);
      if(pba->has_lambda == _TRUE_)
	printf("     -> Omega_Lambda = %g, wished %g\n",
               pvecback[pba->index_bg_rho_lambda]/pvecback[pba->index_bg_rho_crit], pba->Omega0_lambda);
      printf("     -> parameters: [lambda, alpha, A, B] = \n");
      printf("                    [");
      for (i=0; i<pba->scf_parameters_size-1; i++){
        printf("%.3f, ",pba->scf_parameters[i]);
      }
      printf("%.3f]\n",pba->scf_parameters[pba->scf_parameters_size-1]);
    }
  }

  free(pvecback);
  free(pvecback_integration);

  return _SUCCESS_;

}

/**
 * Assign initial values to background integrated variables.
 *
 * @param ppr                  Input: pointer to precision structure
 * @param pba                  Input: pointer to background structure
 * @param pvecback             Input: vector of background quantities used as workspace
 * @param pvecback_integration Output: vector of background quantities to be integrated, returned with proper initial values
 * @return the error status
 */

int background_initial_conditions(
                                  struct precision *ppr,
                                  struct background *pba,
                                  double * pvecback, /* vector with argument pvecback[index_bg] (must be already allocated, normal format is sufficient) */
                                  double * pvecback_integration /* vector with argument pvecback_integration[index_bi] (must be already allocated with size pba->bi_size) */
                                  ) {

  /** Summary: */

  /** - define local variables */

  /* scale factor */
  double a;

  double rho_ncdm, p_ncdm, rho_ncdm_rel_tot=0.;
  double f,Omega_rad, rho_rad;
  int counter,is_early_enough,n_ncdm;
  double scf_lambda;
  double rho_fld_today;
  double w_fld,dw_over_da_fld,integral_fld;
  int n;
  // TK added GDM here 
  /* GDM initial conditions */
  double rho_gdm_today;
  double w_gdm,dw_over_da_gdm,integral_gdm;

  /** - fix initial value of \f$ a \f$ */
  a = ppr->a_ini_over_a_today_default * pba->a_today;
  // printf("a %e \n",a);
  /**  If we have ncdm species, perhaps we need to start earlier
      than the standard value for the species to be relativistic.
      This could happen for some WDM models.
  */

  if (pba->has_ncdm == _TRUE_) {

    for (counter=0; counter < _MAX_IT_; counter++) {

      is_early_enough = _TRUE_;
      rho_ncdm_rel_tot = 0.;

      for (n_ncdm=0; n_ncdm<pba->N_ncdm; n_ncdm++) {

	class_call(background_ncdm_momenta(pba->q_ncdm_bg[n_ncdm],
					   pba->w_ncdm_bg[n_ncdm],
					   pba->q_size_ncdm_bg[n_ncdm],
					   pba->M_ncdm[n_ncdm],
					   pba->factor_ncdm[n_ncdm],
					   pba->a_today/a-1.0,
					   NULL,
					   &rho_ncdm,
					   &p_ncdm,
					   NULL,
					   NULL),
                   pba->error_message,
                   pba->error_message);
	rho_ncdm_rel_tot += 3.*p_ncdm;
	if (fabs(p_ncdm/rho_ncdm-1./3.)>ppr->tol_ncdm_initial_w)
	  is_early_enough = _FALSE_;
      }
      if (is_early_enough == _TRUE_)
	break;
      else
	a *= _SCALE_BACK_;
    }
    class_test(counter == _MAX_IT_,
	       pba->error_message,
	       "Search for initial scale factor a such that all ncdm species are relativistic failed.");
  }

  pvecback_integration[pba->index_bi_a] = a;

  /* Set initial values of {B} variables: */
  Omega_rad = pba->Omega0_g;
  if (pba->has_ur == _TRUE_)
    Omega_rad += pba->Omega0_ur;

  rho_rad = Omega_rad*pow(pba->H0,2)/pow(a/pba->a_today,4);
  if (pba->has_ncdm == _TRUE_){
    /** - We must add the relativistic contribution from NCDM species */
    rho_rad += rho_ncdm_rel_tot;
  }
  if (pba->has_dcdm == _TRUE_){
    /* Remember that the critical density today in CLASS conventions is H0^2 */
    pvecback_integration[pba->index_bi_rho_dcdm] =
      pba->Omega_ini_dcdm*pba->H0*pba->H0*pow(pba->a_today/a,3);
    if (pba->background_verbose > 3)
      printf("Density is %g. a_today=%g. Omega_ini=%g\n",pvecback_integration[pba->index_bi_rho_dcdm],pba->a_today,pba->Omega_ini_dcdm);
  }

  if (pba->has_dr == _TRUE_){
    if (pba->has_dcdm == _TRUE_){
      /**  - f is the critical density fraction of DR. The exact solution is:
       *
       * `f = -Omega_rad+pow(pow(Omega_rad,3./2.)+0.5*pow(a/pba->a_today,6)*pvecback_integration[pba->index_bi_rho_dcdm]*pba->Gamma_dcdm/pow(pba->H0,3),2./3.);`
       *
       * but it is not numerically stable for very small f which is always the case.
       * Instead we use the Taylor expansion of this equation, which is equivalent to
       * ignoring f(a) in the Hubble rate.
       */
      f = 1./3.*pow(a/pba->a_today,6)*pvecback_integration[pba->index_bi_rho_dcdm]*pba->Gamma_dcdm/pow(pba->H0,3)/sqrt(Omega_rad);
      pvecback_integration[pba->index_bi_rho_dr] = f*pba->H0*pba->H0/pow(a/pba->a_today,4);
    }
    else{
      /** There is also a space reserved for a future case where dr is not sourced by dcdm */
      pvecback_integration[pba->index_bi_rho_dr] = 0.0;
    }
  }

  if (pba->has_fld == _TRUE_){
    for(n = 0 ; n<pba->n_fld ; n++){
      /* rho_fld today */
      if(pba->Omega0_fld!=0) rho_fld_today = pba->Omega0_fld * pow(pba->H0,2);
      else rho_fld_today = pba->Omega_many_fld[n] * pow(pba->H0,2);
      /* integrate rho_fld(a) from a_ini to a_0, to get rho_fld(a_ini) given rho_fld(a0) */
      class_call(background_w_fld(pba,a,&w_fld,&dw_over_da_fld,&integral_fld,n), pba->error_message, pba->error_message);

      /* Note: for complicated w_fld(a) functions with no simple
      analytic integral, this is the place were you should compute
      numerically the simple 1d integral [int_{a_ini}^{a_0} 3
      [(1+w_fld)/a] da] (e.g. with the Romberg method?) instead of
      calling background_w_fld */
      int is_log = _TRUE_; 
      double tmp_integral = 0;
      if(pba->w_fld_parametrization == w_free_function) {
        if(pba->w_free_function_from_file == _TRUE_){
          is_log = _FALSE_;
        class_call(romberg_integrate_w_free_function(pba,a,pba->a_today,20,1e-4,&tmp_integral,is_log,n),pba->error_message, pba->error_message);
        integral_fld=tmp_integral;
        // integral_fld = 6.6671e-7;
        }
        else{
          class_call(romberg_integrate_w_free_function(pba,log10(a),0,30,1e-4,&tmp_integral,is_log,n),pba->error_message, pba->error_message);
          // if(pba->w_fld_parametrization == w_free_function) class_call(simpson_integrate_w_free_function(pba,-14,-3,1e6,&integral_fld),pba->error_message, pba->error_message);
          integral_fld=log(10)*tmp_integral; //log10 to log natural conversion
        }
        // is_log = _FALSE_;
        // class_call(romberg_integrate_w_free_function(pba,1e-3,pba->a_today,30,1e-3,&tmp_integral,is_log),pba->error_message, pba->error_message);
        // integral_fld+=tmp_integral;
      }
      else if (pba->w_fld_parametrization == pheno_alternative){
        is_log = _FALSE_;
      class_call(romberg_integrate_w_free_function(pba,a,pba->a_today,30,1e-4,&tmp_integral,is_log,n),pba->error_message, pba->error_message);
      integral_fld=tmp_integral;
      }
      // printf("a ini %e a today %e integral_fld  %e\n", a,pba->a_today, integral_fld);
      // integral_fld = 1; // currently assume the fluid to be negligeable at early time.
      /* rho_fld at initial time */
      pvecback_integration[pba->index_bi_rho_fld+n] = rho_fld_today * exp(integral_fld);
    }
  }

  // TK added GDM here 
  if (pba->has_gdm == _TRUE_){
    /* rho_gdm today */
    rho_gdm_today = pba->Omega0_gdm * pow(pba->H0,2);

    /* integrate rho_fld(a) from a_ini to a_0, to get rho_fld(a_ini) given rho_fld(a0) */
    // Based on fld wa w0 parametrisation 
    class_call(background_w_gdm(pba,a,&w_gdm,&dw_over_da_gdm,&integral_gdm), pba->error_message, pba->error_message);

    // TK the following is important 
    /* Note: for complicated w_fld(a) functions with no simple
    analytic integral, this is the place were you should compute
    numerically the simple 1d integral 
    [int_{a_ini}^{a_0} 3[(1+w_fld)/a] da] 
    (e.g. with the Romberg method?) instead of
    calling background_w_fld */

    int is_log = _TRUE_; 
    double tmp_integral = 0;
    class_call(romberg_integrate_w_gdm(pba,log10(a),0,30,1e-4,&tmp_integral,is_log),pba->error_message, pba->error_message);
    // if(pba->w_fld_parametrization == w_free_function) class_call(simpson_integrate_w_free_function(pba,-14,-3,1e6,&integral_fld),pba->error_message, pba->error_message);
    integral_gdm=log(10)*tmp_integral; //log10 to log natural conversion
      // is_log = _FALSE_;
      // class_call(romberg_integrate_w_gdm(pba,1e-3,pba->a_today,30,1e-3,&tmp_integral,is_log),pba->error_message, pba->error_message);
      // integral_fld+=tmp_integral;
    

    /* rho_gdm at initial time */
    pvecback_integration[pba->index_bi_rho_gdm] = rho_gdm_today * exp(integral_gdm);
    // printf("initial rho_gdm %e\n", pvecback_integration[pba->index_bi_rho_gdm] );
    // printf("rho_gdm today %e\n", rho_gdm_today );
    // printf("exp integral %e\n", exp(integral_gdm) );


  }  



  /** - Fix initial value of \f$ \phi, \phi' \f$
   * set directly in the radiation attractor => fixes the units in terms of rho_ur
   *
   * TODO:
   * - There seems to be some small oscillation when it starts.
   * - Check equations and signs. Sign of phi_prime?
   * - is rho_ur all there is early on?
   */
  if(pba->has_scf == _TRUE_){
    scf_lambda = pba->scf_parameters[0];
    if(pba->attractor_ic_scf == _TRUE_){
      pvecback_integration[pba->index_bi_phi_scf] = -1/scf_lambda*
        log(rho_rad*4./(3*pow(scf_lambda,2)-12))*pba->phi_ini_scf;
      if (3.*pow(scf_lambda,2)-12. < 0){
        /** - --> If there is no attractor solution for scf_lambda, assign some value. Otherwise would give a nan.*/
    	pvecback_integration[pba->index_bi_phi_scf] = 1./scf_lambda;//seems to the work
	if (pba->background_verbose > 0)
	  printf(" No attractor IC for lambda = %.3e ! \n ",scf_lambda);
      }
      pvecback_integration[pba->index_bi_phi_prime_scf] = 2*pvecback_integration[pba->index_bi_a]*
        sqrt(V_scf(pba,pvecback_integration[pba->index_bi_phi_scf]))*pba->phi_prime_ini_scf;
    }
    else{
      printf("Not using attractor initial conditions\n");
      /** - --> If no attractor initial conditions are assigned, gets the provided ones. */
      pvecback_integration[pba->index_bi_phi_scf] = pba->phi_ini_scf;
      pvecback_integration[pba->index_bi_phi_prime_scf] = pba->phi_prime_ini_scf;
    }
    class_test(!isfinite(pvecback_integration[pba->index_bi_phi_scf]) ||
               !isfinite(pvecback_integration[pba->index_bi_phi_scf]),
               pba->error_message,
               "initial phi = %e phi_prime = %e -> check initial conditions",
               pvecback_integration[pba->index_bi_phi_scf],
               pvecback_integration[pba->index_bi_phi_scf]);
  }

  /* Infer pvecback from pvecback_integration */
  class_call(background_functions(pba, pvecback_integration, pba->normal_info, pvecback),
	     pba->error_message,
	     pba->error_message);

  /* Just checking that our initial time indeed is deep enough in the radiation
     dominated regime */
  class_test(fabs(pvecback[pba->index_bg_Omega_r]-1.) > ppr->tol_initial_Omega_r,
	     pba->error_message,
	     "Omega_r = %e, not close enough to 1. Decrease a_ini_over_a_today_default in order to start from radiation domination.",
	     pvecback[pba->index_bg_Omega_r]);

  /** - compute initial proper time, assuming radiation-dominated
      universe since Big Bang and therefore \f$ t=1/(2H) \f$ (good
      approximation for most purposes) */

  class_test(pvecback[pba->index_bg_H] <= 0.,
             pba->error_message,
             "H = %e instead of strictly positive",pvecback[pba->index_bg_H]);

  pvecback_integration[pba->index_bi_time] = 1./(2.* pvecback[pba->index_bg_H]);

  /** - compute initial conformal time, assuming radiation-dominated
      universe since Big Bang and therefore \f$ \tau=1/(aH) \f$
      (good approximation for most purposes) */
  pvecback_integration[pba->index_bi_tau] = 1./(a * pvecback[pba->index_bg_H]);

  /** - compute initial sound horizon, assuming \f$ c_s=1/\sqrt{3} \f$ initially */
  pvecback_integration[pba->index_bi_rs] = pvecback_integration[pba->index_bi_tau]/sqrt(3.);

  /** - set initial value of D and D' in RD. D will be renormalised later, but D' must be correct. */
  pvecback_integration[pba->index_bi_D] = a;
  pvecback_integration[pba->index_bi_D_prime] = 2*pvecback_integration[pba->index_bi_D]*pvecback[pba->index_bg_H];

  return _SUCCESS_;

}

/**
 * Subroutine for formatting background output
 *
 */

int background_output_titles(struct background * pba,
                             char titles[_MAXTITLESTRINGLENGTH_]
                             ){

  /** - Length of the column title should be less than _OUTPUTPRECISION_+6
      to be indented correctly, but it can be as long as . */
  int n;
  char tmp[20];
  // printf("has_fld  %d %d \n", pba->has_fld,pba->has_cdm);
  class_store_columntitle(titles,"z",_TRUE_);
  class_store_columntitle(titles,"proper time [Gyr]",_TRUE_);
  class_store_columntitle(titles,"conf. time [Mpc]",_TRUE_);
  class_store_columntitle(titles,"H [1/Mpc]",_TRUE_);
  class_store_columntitle(titles,"comov. dist.",_TRUE_);
  class_store_columntitle(titles,"ang.diam.dist.",_TRUE_);
  class_store_columntitle(titles,"lum. dist.",_TRUE_);
  class_store_columntitle(titles,"comov.snd.hrz.",_TRUE_);
  class_store_columntitle(titles,"(.)rho_g",_TRUE_);
  class_store_columntitle(titles,"(.)rho_b",_TRUE_);
  class_store_columntitle(titles,"(.)rho_cdm",pba->has_cdm);
  // TK this is where sequence of columns has to match with output data
  class_store_columntitle(titles,"(.)rho_gdm",pba->has_gdm);
  class_store_columntitle(titles,"(.)w_gdm",pba->has_gdm);
  class_store_columntitle(titles,"(.)dw_gdm",pba->has_gdm);
  if (pba->has_ncdm == _TRUE_){
    for (n=0; n<pba->N_ncdm; n++){
      sprintf(tmp,"(.)rho_ncdm[%d]",n);
      class_store_columntitle(titles,tmp,_TRUE_);
      sprintf(tmp,"(.)p_ncdm[%d]",n);
      class_store_columntitle(titles,tmp,_TRUE_);
    }
  }
  class_store_columntitle(titles,"(.)rho_lambda",pba->has_lambda);
  if (pba->has_fld == _TRUE_){
    for (n=0; n<pba->n_fld; n++){
      sprintf(tmp,"(.)rho_fld[%d]",n);
      class_store_columntitle(titles,tmp,_TRUE_);
      sprintf(tmp,"(.)w_fld[%d]",n);
      class_store_columntitle(titles,tmp,_TRUE_);
      if(pba->w_free_function_file_is_ca2 == _TRUE_){
        sprintf(tmp,"(.)ca2_fld[%d]",n);
        class_store_columntitle(titles,tmp,_TRUE_);
      }
      else if(pba->w_free_function_file_is_dw_over_1_p_w == _TRUE_){
        sprintf(tmp,"(.)dw_over_w_fld[%d]",n);
        class_store_columntitle(titles,tmp,_TRUE_);
      }
      else{
      sprintf(tmp,"(.)dw_fld[%d]",n);
      class_store_columntitle(titles,tmp,_TRUE_);
      }

    }
  }
  class_store_columntitle(titles,"(.)rho_ur",pba->has_ur);
  class_store_columntitle(titles,"(.)rho_crit",_TRUE_);
  class_store_columntitle(titles,"(.)rho_dcdm",pba->has_dcdm);
  class_store_columntitle(titles,"(.)rho_dr",pba->has_dr);

  class_store_columntitle(titles,"(.)rho_scf",pba->has_scf);
  class_store_columntitle(titles,"(.)p_scf",pba->has_scf);
  class_store_columntitle(titles,"phi_scf",pba->has_scf);
  class_store_columntitle(titles,"phi'_scf",pba->has_scf);
  class_store_columntitle(titles,"V_scf",pba->has_scf);
  class_store_columntitle(titles,"V'_scf",pba->has_scf);
  class_store_columntitle(titles,"V''_scf",pba->has_scf);

  class_store_columntitle(titles,"gr.fac. D",_TRUE_);
  class_store_columntitle(titles,"gr.fac. f",_TRUE_);

  return _SUCCESS_;
}

int background_output_data(
                           struct background *pba,
                           int number_of_titles,
                           double *data){
  int index_tau, storeidx, n;
  double *dataptr, *pvecback;

  /** Stores quantities */
  for (index_tau=0; index_tau<pba->bt_size; index_tau++){
    dataptr = data + index_tau*number_of_titles;
    pvecback = pba->background_table + index_tau*pba->bg_size;
    storeidx = 0;

    class_store_double(dataptr,pba->a_today/pvecback[pba->index_bg_a]-1.,_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_time]/_Gyr_over_Mpc_,_TRUE_,storeidx);
    class_store_double(dataptr,pba->conformal_age-pvecback[pba->index_bg_conf_distance],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_H],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_conf_distance],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_ang_distance],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_lum_distance],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rs],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rho_g],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rho_b],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rho_cdm],pba->has_cdm,storeidx);
    /* TK added this to store rho_GDM index */
    // TK This is where the sequence of columns has to match with output titles 
    class_store_double(dataptr,pvecback[pba->index_bg_rho_gdm],pba->has_gdm,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_w_gdm],pba->has_gdm,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_dw_gdm],pba->has_gdm,storeidx);

    if (pba->has_ncdm == _TRUE_){
      for (n=0; n<pba->N_ncdm; n++){
        class_store_double(dataptr,pvecback[pba->index_bg_rho_ncdm1+n],_TRUE_,storeidx);
        class_store_double(dataptr,pvecback[pba->index_bg_p_ncdm1+n],_TRUE_,storeidx);
      }
    }
    class_store_double(dataptr,pvecback[pba->index_bg_rho_lambda],pba->has_lambda,storeidx);
    if(pba->has_fld == _TRUE_){
      for (n=0; n<pba->n_fld; n++){
        class_store_double(dataptr,pvecback[pba->index_bg_rho_fld+n],pba->has_fld,storeidx);
        class_store_double(dataptr,pvecback[pba->index_bg_w_fld+n],pba->has_fld,storeidx);
        if(pba->w_free_function_file_is_dw_over_1_p_w == _TRUE_){
          class_store_double(dataptr,pvecback[pba->index_bg_dw_fld+n],pba->has_fld,storeidx);
        }
        else if(pba->w_free_function_file_is_ca2 == _TRUE_){
          class_store_double(dataptr,pvecback[pba->index_bg_dw_fld+n],pba->has_fld,storeidx);
        }
        else {
          class_store_double(dataptr,pvecback[pba->index_bg_dw_fld+n],pba->has_fld,storeidx);
        }
      }
    }

    class_store_double(dataptr,pvecback[pba->index_bg_rho_ur],pba->has_ur,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rho_crit],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rho_dcdm],pba->has_dcdm,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_rho_dr],pba->has_dr,storeidx);

    class_store_double(dataptr,pvecback[pba->index_bg_rho_scf],pba->has_scf,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_p_scf],pba->has_scf,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_phi_scf],pba->has_scf,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_phi_prime_scf],pba->has_scf,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_V_scf],pba->has_scf,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_dV_scf],pba->has_scf,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_ddV_scf],pba->has_scf,storeidx);

    class_store_double(dataptr,pvecback[pba->index_bg_D],_TRUE_,storeidx);
    class_store_double(dataptr,pvecback[pba->index_bg_f],_TRUE_,storeidx);
  }

  return _SUCCESS_;
}


/**
 * Subroutine evaluating the derivative with respect to conformal time
 * of quantities which are integrated (a, t, etc).
 *
 * This is one of the few functions in the code which is passed to
 * the generic_integrator() routine.  Since generic_integrator()
 * should work with functions passed from various modules, the format
 * of the arguments is a bit special:
 *
 * - fixed input parameters and workspaces are passed through a generic
 * pointer. Here, this is just a pointer to the background structure
 * and to a background vector, but generic_integrator() doesn't know
 * its fine structure.
 *
 * - the error management is a bit special: errors are not written as
 * usual to pba->error_message, but to a generic error_message passed
 * in the list of arguments.
 *
 * @param tau                      Input: conformal time
 * @param y                        Input: vector of variable
 * @param dy                       Output: its derivative (already allocated)
 * @param parameters_and_workspace Input: pointer to fixed parameters (e.g. indices)
 * @param error_message            Output: error message
 */
int background_derivs(
                      double tau,
                      double* y, /* vector with argument y[index_bi] (must be already allocated with size pba->bi_size) */
                      double* dy, /* vector with argument dy[index_bi]
                                     (must be already allocated with
                                     size pba->bi_size) */
                      void * parameters_and_workspace,
                      ErrorMsg error_message
                      ) {

  /** Summary: */

  /** - define local variables */

  struct background_parameters_and_workspace * pbpaw;
  struct background * pba;
  double * pvecback, a, H, rho_M;
  // TK added GDM parameters here 
  double w_gdm, dw_over_da_gdm, integral_gdm;
  int n;
  pbpaw = parameters_and_workspace;
  pba =  pbpaw->pba;
  pvecback = pbpaw->pvecback;

  /** - calculate functions of \f$ a \f$ with background_functions() */
  class_call(background_functions(pba, y, pba->normal_info, pvecback),
             pba->error_message,
             error_message);

  /** - Short hand notation */
  a = y[pba->index_bi_a];
  H = pvecback[pba->index_bg_H];

  /** - calculate \f$ a'=a^2 H \f$ */
  dy[pba->index_bi_a] = y[pba->index_bi_a] * y[pba->index_bi_a] * pvecback[pba->index_bg_H];

  /** - calculate \f$ t' = a \f$ */
  dy[pba->index_bi_time] = y[pba->index_bi_a];

  class_test(pvecback[pba->index_bg_rho_g] <= 0.,
             error_message,
             "rho_g = %e instead of strictly positive",pvecback[pba->index_bg_rho_g]);

  /** - calculate \f$ rs' = c_s \f$*/
  dy[pba->index_bi_rs] = 1./sqrt(3.*(1.+3.*pvecback[pba->index_bg_rho_b]/4./pvecback[pba->index_bg_rho_g]))*sqrt(1.-pba->K*y[pba->index_bi_rs]*y[pba->index_bi_rs]); // TBC: curvature correction

  /** - solve second order growth equation  \f$ [D''(\tau)=-aHD'(\tau)+3/2 a^2 \rho_M D(\tau) \f$ */
  rho_M = pvecback[pba->index_bg_rho_b];
  if (pba->has_cdm)
    rho_M += pvecback[pba->index_bg_rho_cdm];

  // TK added GDM here
  // Not sure about this one
  if (pba->has_gdm == _TRUE_) {
    /** - Compute gdm density \f$ \rho' = -3aH (1+w_{gdm}(a)) \rho \f$ */
    class_call(background_w_gdm(pba,a,&w_gdm,&dw_over_da_gdm,&integral_gdm), pba->error_message, pba->error_message);
    dy[pba->index_bi_rho_gdm] = -3.*y[pba->index_bi_a]*pvecback[pba->index_bg_H]*(1.+pvecback[pba->index_bg_w_gdm])*y[pba->index_bi_rho_gdm];
    if(w_gdm < 0.33 && w_gdm >= 0.) { rho_M += pvecback[pba->index_bg_rho_gdm]; } // TK has bg_rho_gdm been assigned yet ?????? 
  } 


  dy[pba->index_bi_D] = y[pba->index_bi_D_prime];
  dy[pba->index_bi_D_prime] = -a*H*y[pba->index_bi_D_prime] + 1.5*a*a*rho_M*y[pba->index_bi_D];

  if (pba->has_dcdm == _TRUE_){
    /** - compute dcdm density \f$ \rho' = -3aH \rho - a \Gamma \rho \f$*/
    dy[pba->index_bi_rho_dcdm] = -3.*y[pba->index_bi_a]*pvecback[pba->index_bg_H]*y[pba->index_bi_rho_dcdm]-
      y[pba->index_bi_a]*pba->Gamma_dcdm*y[pba->index_bi_rho_dcdm];
  }

  if ((pba->has_dcdm == _TRUE_) && (pba->has_dr == _TRUE_)){
    /** - Compute dr density \f$ \rho' = -4aH \rho - a \Gamma \rho \f$ */
    dy[pba->index_bi_rho_dr] = -4.*y[pba->index_bi_a]*pvecback[pba->index_bg_H]*y[pba->index_bi_rho_dr]+
      y[pba->index_bi_a]*pba->Gamma_dcdm*y[pba->index_bi_rho_dcdm];
  }

  if (pba->has_fld == _TRUE_) {
    /** - Compute fld density \f$ \rho' = -3aH (1+w_{fld}(a)) \rho \f$ */
    for(n = 0; n < pba->n_fld; n++){
      dy[pba->index_bi_rho_fld+n] = -3.*y[pba->index_bi_a]*pvecback[pba->index_bg_H]*(1.+pvecback[pba->index_bg_w_fld+n])*y[pba->index_bi_rho_fld+n];
    }
  }

  if (pba->has_scf == _TRUE_){
    /** - Scalar field equation: \f$ \phi'' + 2 a H \phi' + a^2 dV = 0 \f$  (note H is wrt cosmic time) */
    dy[pba->index_bi_phi_scf] = y[pba->index_bi_phi_prime_scf];
    dy[pba->index_bi_phi_prime_scf] = - y[pba->index_bi_a]*
      (2*pvecback[pba->index_bg_H]*y[pba->index_bi_phi_prime_scf]
       + y[pba->index_bi_a]*dV_scf(pba,y[pba->index_bi_phi_scf])) ;
  }


  return _SUCCESS_;

}

/**
 * Scalar field potential and its derivatives with respect to the field _scf
 * For Albrecht & Skordis model: 9908085
 * - \f$ V = V_{p_{scf}}*V_{e_{scf}} \f$
 * - \f$ V_e =  \exp(-\lambda \phi) \f$ (exponential)
 * - \f$ V_p = (\phi - B)^\alpha + A \f$ (polynomial bump)
 *
 * TODO:
 * - Add some functionality to include different models/potentials (tuning would be difficult, though)
 * - Generalize to Kessence/Horndeski/PPF and/or couplings
 * - A default module to numerically compute the derivatives when no analytic functions are given should be added.
 * - Numerical derivatives may further serve as a consistency check.
 *
 */

/**
 *
 * The units of phi, tau in the derivatives and the potential V are the following:
 * - phi is given in units of the reduced Planck mass \f$ m_{pl} = (8 \pi G)^{(-1/2)}\f$
 * - tau in the derivative is given in units of Mpc.
 * - the potential \f$ V(\phi) \f$ is given in units of \f$ m_{pl}^2/Mpc^2 \f$.
 * With this convention, we have
 * \f$ \rho^{class} = (8 \pi G)/3 \rho^{physical} = 1/(3 m_{pl}^2) \rho^{physical} = 1/3 * [ 1/(2a^2) (\phi')^2 + V(\phi) ] \f$
    and \f$ \rho^{class} \f$ has the proper dimension \f$ Mpc^-2 \f$.
 */

double V_e_scf(struct background *pba,
               double phi
               ) {
  double scf_lambda = pba->scf_parameters[0];
  //  double scf_alpha  = pba->scf_parameters[1];
  //  double scf_A      = pba->scf_parameters[2];
  //  double scf_B      = pba->scf_parameters[3];

  return  exp(-scf_lambda*phi);
}

double dV_e_scf(struct background *pba,
                double phi
                ) {
  double scf_lambda = pba->scf_parameters[0];
  //  double scf_alpha  = pba->scf_parameters[1];
  //  double scf_A      = pba->scf_parameters[2];
  //  double scf_B      = pba->scf_parameters[3];

  return -scf_lambda*V_scf(pba,phi);
}

double ddV_e_scf(struct background *pba,
                 double phi
                 ) {
  double scf_lambda = pba->scf_parameters[0];
  //  double scf_alpha  = pba->scf_parameters[1];
  //  double scf_A      = pba->scf_parameters[2];
  //  double scf_B      = pba->scf_parameters[3];

  return pow(-scf_lambda,2)*V_scf(pba,phi);
}


/** parameters and functions for the polynomial coefficient
 * \f$ V_p = (\phi - B)^\alpha + A \f$(polynomial bump)
 *
 * double scf_alpha = 2;
 *
 * double scf_B = 34.8;
 *
 * double scf_A = 0.01; (values for their Figure 2)
 */

double V_p_scf(
               struct background *pba,
               double phi) {
  //  double scf_lambda = pba->scf_parameters[0];
  double scf_alpha  = pba->scf_parameters[1];
  double scf_A      = pba->scf_parameters[2];
  double scf_B      = pba->scf_parameters[3];

  return  pow(phi - scf_B,  scf_alpha) +  scf_A;
}

double dV_p_scf(
                struct background *pba,
                double phi) {

  //  double scf_lambda = pba->scf_parameters[0];
  double scf_alpha  = pba->scf_parameters[1];
  //  double scf_A      = pba->scf_parameters[2];
  double scf_B      = pba->scf_parameters[3];

  return   scf_alpha*pow(phi -  scf_B,  scf_alpha - 1);
}

double ddV_p_scf(
                 struct background *pba,
                 double phi) {
  //  double scf_lambda = pba->scf_parameters[0];
  double scf_alpha  = pba->scf_parameters[1];
  //  double scf_A      = pba->scf_parameters[2];
  double scf_B      = pba->scf_parameters[3];

  return  scf_alpha*(scf_alpha - 1.)*pow(phi -  scf_B,  scf_alpha - 2);
}

/** Fianlly we can obtain the overall potential \f$ V = V_p*V_e \f$
 */

double V_scf(
             struct background *pba,
             double phi) {
  return  V_e_scf(pba,phi)*V_p_scf(pba,phi);
}

double dV_scf(
              struct background *pba,
	      double phi) {
  return dV_e_scf(pba,phi)*V_p_scf(pba,phi) + V_e_scf(pba,phi)*dV_p_scf(pba,phi);
}

double ddV_scf(
               struct background *pba,
               double phi) {
  return ddV_e_scf(pba,phi)*V_p_scf(pba,phi) + 2*dV_e_scf(pba,phi)*dV_p_scf(pba,phi) + V_e_scf(pba,phi)*ddV_p_scf(pba,phi);
}
