#include <iostream>
#include "tools.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using std::vector;

Tools::Tools() {}

Tools::~Tools() {}

VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
                              const vector<VectorXd> &ground_truth) {
  /**
  TODO:
    * Calculate the RMSE here.
  */

  VectorXd rmse(4); 
  rmse << 0,0,0,0;

  if (estimations.size() != ground_truth.size() 
        || estimations.size() == 0) 
  {
    std::cout << "invalid estimation or ground truth data" << endl; 
    return rmse;          
  }

  for (int i=0; i < estimations.size(); i++) 
  {
    rmse += (estimations[i] - ground_truth[i]).cwiseProduct(estimations[i] - ground_truth[i]);
  }

  rmse = rmse / estimations.size();

  return rmse.array().sqrt();
  
}