#ifndef RIEMANN_SOLVER_H
#define RIEMANN_SOLVER_H

#include "helper.h"
#include "problem_description.h"

#include <deal.II/base/point.h>
#include <deal.II/base/tensor.h>

#include <functional>

namespace grendel
{

  /**
   * A fast approximative Riemann problem solver for the nD compressible
   * Euler problem.
   *
   * FIXME: Desciption
   */
  template <int dim, typename Number = double>
  class RiemannSolver
  {
  public:
    static constexpr unsigned int problem_dimension =
        ProblemDescription<dim, Number>::problem_dimension;

    using rank1_type = typename ProblemDescription<dim, Number>::rank1_type;

    using ScalarNumber = typename get_value_type<Number>::type;

    /*
     * Options:
     */

    static constexpr ScalarNumber newton_eps_ = 1.0e-10;

    static constexpr unsigned int newton_max_iter_ = 0;

    /**
     * For two given states U_i a U_j and a (normalized) "direction" n_ij
     * compute an estimation of an upper bound for lambda.
     *
     * See [1], page 915, Algorithm 1
     *
     * Returns a tuple consisting of lambda max and the number of Newton
     * iterations used in the solver to find it.
     *
     * References:
     *   [1] J.-L. Guermond, B. Popov. Fast estimation from above for the
     *       maximum wave speed in the Riemann problem for the Euler equations.
     */
    static std::tuple<Number /*lambda_max*/,
                      Number /*p_star*/,
                      unsigned int /*iteration*/>
    compute(const rank1_type U_i,
            const rank1_type U_j,
            const dealii::Tensor<1, dim, Number> &n_ij);


    /**
     * Variant of above function that takes two arrays as input describing
     * the "1D Riemann data" instead of two nD states.
     */
    // Remark: We use the  non-type template parameter max_iter in order to
    // change the compile-time constant in the test
    template <unsigned int max_iter = newton_max_iter_>
    static std::tuple<Number /*lambda_max*/,
                      Number /*p_star*/,
                      unsigned int /*iteration*/>
    compute(const std::array<Number, 6> &riemann_data_i,
            const std::array<Number, 6> &riemann_data_j);
  };

} /* namespace grendel */

#endif /* RIEMANN_SOLVER_H */
