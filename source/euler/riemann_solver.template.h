//
// SPDX-License-Identifier: MIT
// Copyright (C) 2020 - 2022 by the ryujin authors
//

#pragma once

#include <compile_time_options.h>

#include "riemann_solver.h"

#include <newton.h>
#include <simd.h>

namespace ryujin
{
  namespace Euler
  {
    /*
     * The approximate Riemann solver is based on a function phi(p) that is
     * montone increasing in p, concave down and whose (weak) third
     * derivative is non-negative and locally bounded [1, p. 912]. Because we
     * actually do not perform any iteration for computing our wavespeed
     * estimate we can get away by only implementing a specialized variant of
     * the phi function that computes phi(p_max). It inlines the
     * implementation of the "f" function and eliminates all unnecessary
     * branches in "f".
     *
     * Cost: 0x pow, 2x division, 2x sqrt
     */
    template <int dim, typename Number>
    DEAL_II_ALWAYS_INLINE inline Number
    RiemannSolver<dim, Number>::phi_of_p_max(
        const primitive_type &riemann_data_i,
        const primitive_type &riemann_data_j) const
    {
      const auto &[rho_i, u_i, p_i, a_i] = riemann_data_i;
      const auto &[rho_j, u_j, p_j, a_j] = riemann_data_j;

      const Number p_max = std::max(p_i, p_j);

      const auto gamma = hyperbolic_system.gamma();

      const Number radicand_inverse_i = ScalarNumber(0.5) * rho_i *
                                        ((gamma + ScalarNumber(1.)) * p_max +
                                         (gamma - ScalarNumber(1.)) * p_i);

      const Number value_i = (p_max - p_i) / std::sqrt(radicand_inverse_i);

      const Number radicand_inverse_j = ScalarNumber(0.5) * rho_j *
                                        ((gamma + ScalarNumber(1.)) * p_max +
                                         (gamma - ScalarNumber(1.)) * p_j);

      const Number value_j = (p_max - p_j) / std::sqrt(radicand_inverse_j);

      return value_i + value_j + u_j - u_i;
    }


    /*
     * Next we construct approximations for the two extreme wave speeds of
     * the Riemann fan [1, p. 912, (3.7) + (3.8)] and compute an upper bound
     * lambda_max of the maximal wave speed:
     */


    /**
     * see [1], page 912, (3.7)
     *
     * Cost: 0x pow, 1x division, 1x sqrt
     */
    template <int dim, typename Number>
    DEAL_II_ALWAYS_INLINE inline Number
    RiemannSolver<dim, Number>::lambda1_minus(
        const primitive_type &riemann_data, const Number p_star) const
    {
      const auto &[rho, u, p, a] = riemann_data;
      const auto inv_p = ScalarNumber(1.0) / p;

      const auto gamma = hyperbolic_system.gamma();
      const auto gamma_inverse = hyperbolic_system.gamma_inverse();
      const auto factor =
          (gamma + ScalarNumber(1.0)) * ScalarNumber(0.5) * gamma_inverse;
      const Number tmp = positive_part((p_star - p) * inv_p);

      return u - a * std::sqrt(ScalarNumber(1.0) + factor * tmp);
    }


    /**
     * see [1], page 912, (3.8)
     *
     * Cost: 0x pow, 1x division, 1x sqrt
     */
    template <int dim, typename Number>
    DEAL_II_ALWAYS_INLINE inline Number
    RiemannSolver<dim, Number>::lambda3_plus(
        const primitive_type &primitive_state, const Number p_star) const
    {
      const auto &[rho, u, p, a] = primitive_state;
      const auto inv_p = ScalarNumber(1.0) / p;

      const auto gamma = hyperbolic_system.gamma();
      const auto gamma_inverse = hyperbolic_system.gamma_inverse();
      const Number factor =
          (gamma + ScalarNumber(1.0)) * ScalarNumber(0.5) * gamma_inverse;
      const Number tmp = positive_part((p_star - p) * inv_p);
      return u + a * std::sqrt(Number(1.0) + factor * tmp);
    }


    /**
     * For two given primitive states <code>riemann_data_i</code> and
     * <code>riemann_data_j</code>, and a guess p_2, compute an upper bound
     * for lambda.
     *
     * This is the same lambda_max as computed by compute_gap. The function
     * simply avoids a number of unnecessary computations (in case we do
     * not need to know the gap).
     *
     * Cost: 0x pow, 2x division, 2x sqrt
     */
    template <int dim, typename Number>
    DEAL_II_ALWAYS_INLINE inline Number
    RiemannSolver<dim, Number>::compute_lambda(
        const primitive_type &riemann_data_i,
        const primitive_type &riemann_data_j,
        const Number p_star) const
    {
      const Number nu_11 = lambda1_minus(riemann_data_i, p_star);
      const Number nu_32 = lambda3_plus(riemann_data_j, p_star);

      return std::max(positive_part(nu_32), negative_part(nu_11));
    }


    /**
     * Two-rarefaction approximation to p_star computed for two primitive
     * states <code>riemann_data_i</code> and <code>riemann_data_j</code>.
     *
     * See [1], page 914, (4.3)
     *
     * Cost: 2x pow, 2x division, 0x sqrt
     */
    template <int dim, typename Number>
    DEAL_II_ALWAYS_INLINE inline Number
    RiemannSolver<dim, Number>::p_star_two_rarefaction(
        const primitive_type &riemann_data_i,
        const primitive_type &riemann_data_j) const
    {
      const auto &[rho_i, u_i, p_i, a_i] = riemann_data_i;
      const auto &[rho_j, u_j, p_j, a_j] = riemann_data_j;
      const auto inv_p_j = ScalarNumber(1.) / p_j;

      /*
       * Nota bene (cf. [1, (4.3)]):
       *   a_Z^0 * sqrt(1 - b * rho_Z) = a_Z * (1 - b * rho_Z)
       * We have computed a_Z already, so we are simply going to use this
       * identity below:
       */

      const auto &gamma = hyperbolic_system.gamma();
      const auto factor = (gamma - ScalarNumber(1.)) * ScalarNumber(0.5);

      /*
       * Nota bene (cf. [1, (3.6)]: The condition "numerator > 0" is the
       * well-known non-vacuum condition. In case we encounter numerator <= 0
       * then p_star = 0 is the correct pressure to compute the wave speed.
       * Therefore, all we have to do is to take the positive part of the
       * expression:
       */
      const auto &gamma_inverse = hyperbolic_system.gamma_inverse();
      const auto &gm1_inverse = hyperbolic_system.gamma_minus_one_inverse();

      const Number numerator = positive_part(a_i + a_j - factor * (u_j - u_i));
      const Number denominator =
          a_i * ryujin::pow(p_i * inv_p_j, -factor * gamma_inverse) + a_j;

      const auto exponent = ScalarNumber(2.0) * gamma * gm1_inverse;

      return p_j * ryujin::pow(numerator / denominator, exponent);
    }


    template <int dim, typename Number>
    Number RiemannSolver<dim, Number>::compute(
        const primitive_type &riemann_data_i,
        const primitive_type &riemann_data_j) const
    {
      /*
       * For exactly solving the Riemann problem we need to start with a
       * good upper and lower bound, p_1 <= p_star <= p_2, for finding
       * phi(p_star) == 0. This implies that we have to ensure that
       * phi(p_2) >= 0 and phi(p_1) <= 0.
       *
       * Instead of solving the Riemann problem exactly, however we will
       * simply use the upper bound p_2 (with p_2 >= p_star) to compute
       * lambda_max and return the estimate.
       *
       * We will use three candidates, p_min, p_max and the two rarefaction
       * approximation p_star_tilde. We have (up to round-off errors) that
       * phi(p_star_tilde) >= 0. So this is a safe upper bound, it might
       * just be too large.
       *
       * Depending on the sign of phi(p_max) we select the following ranges:
       *
       *   phi(p_max) <  0:
       *     p_1  <-  p_max   and   p_2  <-  p_star_tilde
       *
       *   phi(p_max) >= 0:
       *     p_1  <-  p_min   and   p_2  <-  min(p_max, p_star_tilde)
       *
       * Nota bene:
       *
       *  - The special case phi(p_max) == 0 as discussed in [1] is already
       *    contained in the second condition.
       *
       *  - In principle, we would have to treat the case phi(p_min) > 0 as
       *    well. This corresponds to two expansion waves and a good
       *    estimate for the wavespeed is obtained by simply computing
       *    lambda_max with p_2 = 0.
       *
       *    However, it turns out that numerically in this case we will
       *    have
       *
       *      0 < p_star <= p_star_tilde <= p_min <= p_max.
       *
       *    So it is sufficient to end up with p_2 = p_star_tilde (!!) to
       *    compute the exact same wave speed as for p_2 = 0.
       *
       *    Note: If for some reason p_star should be computed exactly,
       *    then p_1 has to be set to zero. This can be done efficiently by
       *    simply checking for p_2 < p_1 and setting p_1 <- 0 if
       *    necessary.
       */

      const Number p_max = std::max(riemann_data_i[2], riemann_data_j[2]);

      const Number p_star_tilde =
          p_star_two_rarefaction(riemann_data_i, riemann_data_j);

      const Number phi_p_max = phi_of_p_max(riemann_data_i, riemann_data_j);

      const Number p_2 =
          dealii::compare_and_apply_mask<dealii::SIMDComparison::less_than>(
              phi_p_max,
              Number(0.),
              p_star_tilde,
              std::min(p_max, p_star_tilde));

      return compute_lambda(riemann_data_i, riemann_data_j, p_2);
    }

  } // namespace Euler
} // namespace ryujin
