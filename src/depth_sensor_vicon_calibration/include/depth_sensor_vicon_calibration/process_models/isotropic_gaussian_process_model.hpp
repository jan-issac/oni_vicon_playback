#ifndef POSE_FILTER_ISOTROPIC_GAUSSIAN_PROCESS_MODEL_
#define POSE_FILTER_ISOTROPIC_GAUSSIAN_PROCESS_MODEL_

#include <boost/shared_ptr.hpp>
#include <vector>
#include <Eigen/Core>

#include <boost/random/normal_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>

#include "process_model.hpp"

namespace proc_mod
{

class IsotropicGaussianProcessModel: public ProcessModel
{
public:
	IsotropicGaussianProcessModel(float angle_sigma, float trans_sigma, Eigen::Vector3f rot_center);
	virtual ~IsotropicGaussianProcessModel();

	virtual std::vector<float> Sample(std::vector<float> initial_state, double delta_time, std::vector<float> controls = std::vector<float>(0));

private:
	boost::mt19937 generator_;
	boost::normal_distribution<> angle_distr_;
	boost::normal_distribution<> trans_distr_;
	boost::variate_generator<boost::mt19937&, boost::normal_distribution<> > angle_generator_;
	boost::variate_generator<boost::mt19937&, boost::normal_distribution<> > trans_generator_;

	Eigen::Vector3f rot_center_;
};

}




#endif
