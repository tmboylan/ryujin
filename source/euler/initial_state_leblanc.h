//
// SPDX-License-Identifier: MIT
// Copyright (C) 2020 - 2022 by the ryujin authors
//

#pragma once

#include <initial_state_library.h>

namespace ryujin
{
  namespace EulerInitialStates
  {
    /**
     * Initial configuration for the LeBlanc shock tube
     * @todo Documentation
     *
     * @ingroup EulerEquations
     */

    template <typename Description, int dim, typename Number>
    class LeBlanc : public InitialState<Description, dim, Number>
    {
    public:
      using HyperbolicSystem = typename Description::HyperbolicSystem;
      using HyperbolicSystemView =
          typename HyperbolicSystem::template View<dim, Number>;
      using state_type = typename HyperbolicSystemView::state_type;

      LeBlanc(const HyperbolicSystem &hyperbolic_system,
              const std::string subsection)
          : InitialState<Description, dim, Number>("leblanc", subsection)
          , hyperbolic_system_(hyperbolic_system)
      {
      } /* Constructor */

      state_type compute(const dealii::Point<dim> &point, Number t) final
      {
        /*
         * The LeBlanc shock tube:
         */

        /* Initial left and right states (rho, u, p): */
        using state_type_1d = dealii::Tensor<1, 3, Number>;
        const state_type_1d primitive_left{{1.0, 0.0, 1.0 / 15.0}};
        const state_type_1d primitive_right{{0.001, 0.0, 2.0 / 3.0 * 1.e-10}};

        /* The intermediate wave-speeds appearing on the Riemann fan: */
        constexpr Number rarefaction_speed = 0.49578489518897934;
        constexpr Number contact_velocity = 0.62183867139173454;
        constexpr Number right_shock_speed = 0.82911836253346982;

        /*
         * Velocity and pressure are constant across the middle discontinuity,
         * only the density jumps: it's a contact wave!
         */
        constexpr Number pre_contact_density = 5.4079335349316249e-02;
        constexpr Number post_contact_density = 3.9999980604299963e-03;
        constexpr Number contact_pressure = 0.51557792765096996e-03;

        state_type_1d result;
        const double &x = point[0];

        if (x <= -1.0 / 3.0 * t) {
          /* Left state: */
          result = primitive_left;

        } else if (x < rarefaction_speed * t) {
          /* Expansion data (with self-similar variable chi): */
          const double chi = x / t;
          result[0] = std::pow(0.75 - 0.75 * chi, 3.0);
          result[1] = 0.75 * (1.0 / 3.0 + chi);
          result[2] = (1.0 / 15.0) * std::pow(0.75 - 0.75 * chi, 5.0);

        } else if (x < contact_velocity * t) {
          result[0] = pre_contact_density;
          result[1] = contact_velocity;
          result[2] = contact_pressure;

        } else if (x < right_shock_speed * t) {
          /* Contact-wave data (velocity and pressure are continuous): */
          result[0] = post_contact_density;
          result[1] = contact_velocity;
          result[2] = contact_pressure;

        } else {
          /* Right state: */
          result = primitive_right;
        }

        // FIXME: update primitive

        return hyperbolic_system_.from_primitive_state(
            hyperbolic_system_.expand_state(result));
      }

    private:
      const HyperbolicSystemView hyperbolic_system_;
    };
  } // namespace EulerInitialStates
} // namespace ryujin
