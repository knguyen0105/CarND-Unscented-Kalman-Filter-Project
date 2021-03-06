#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

#define EPS 0.001

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  is_initialized_ = false;
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;
  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;
  // initial state vector
  x_ = VectorXd(5);
  // initial covariance matrix
  P_ = MatrixXd(5, 5);
  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.5;
  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.57;
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;
  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;
  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;
  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;
  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  // Init noise covariances
  R_radar_ = MatrixXd(3, 3);
  R_radar_ << pow(std_radr_,2), 0, 0,
              0, pow(std_radphi_,2), 0,
              0, 0,pow(std_radrd_,2);

  R_lidar_ = MatrixXd(2, 2);
  R_lidar_ << pow(std_laspx_,2),0,
              0,pow(std_laspy_,2);  

  n_x_ = x_.size();
  n_aug_ = n_x_ + 2; 
  n_sig_ = 2 * n_aug_ + 1;
  
  // Init predicted signma points matrix
  Xsig_pred_ = MatrixXd(n_x_, n_sig_);

  lambda_ = 3 - n_aug_;
  // Weights of sigma points
  weights_ = VectorXd(n_sig_);
  

}

UKF::~UKF() {}



/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage measurement_pack) {

  if (!is_initialized_) {
  
    P_ << 1, 0, 0, 0, 0,
          0, 1, 0, 0, 0,
          0, 0, 1, 0, 0,
          0, 0, 0, 1, 0,
          0, 0, 0, 0, 1;
    if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR) {
      
      // Convert radar from polar to cartesian 
      float rho = measurement_pack.raw_measurements_[0]; 
      float phi = measurement_pack.raw_measurements_[1]; 
      float rho_dot = measurement_pack.raw_measurements_[2]; 

      float px = rho * cos(phi); 
      float py = rho * sin(phi);
      float vx = rho_dot * cos(phi);
      float vy = rho_dot * sin(phi);
      float v  = sqrt(vx * vx + vy * vy);
      x_ << px, py, v, 0, 0;
    }
    else if (measurement_pack.sensor_type_ == MeasurementPackage::LASER) {

      x_ << measurement_pack.raw_measurements_[0], measurement_pack.raw_measurements_[1], 0, 0, 0;
      
      //avoid division by zero 
      if (fabs(x_(0)) < EPS && fabs(x_(1)) < EPS){
    		x_(0) = EPS;
    		x_(1) = EPS;
	    }
    }


    time_us_ = measurement_pack.timestamp_;
    
    // Initialize weights
    weights_(0) = lambda_ / (lambda_ + n_aug_);
    for (int i = 1; i < weights_.size(); i++) {
        weights_(i) = 0.5 / (n_aug_ + lambda_);
    }
     
    is_initialized_ = true;
    return;
  }
  

  double dt = (measurement_pack.timestamp_ - time_us_) / 1000000.0;
  time_us_ = measurement_pack.timestamp_;

  Prediction(dt);
  
  if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR && use_radar_) {
      UpdateRadar(measurement_pack);
    }
  if (measurement_pack.sensor_type_ == MeasurementPackage::LASER && use_laser_) {
      UpdateLidar(measurement_pack);
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  double delta_t2 = pow(delta_t,2);

  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);  
  MatrixXd Xsig_aug = MatrixXd(n_aug_, n_sig_);
  
  // Fill x_aug and P_aug
  x_aug.fill(0.0);
  x_aug.head(n_x_) = x_;
  P_aug.fill(0);
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(5,5) = pow(std_a_,2);
  P_aug(6,6) = pow(std_yawdd_,2);

  // Square root of P
  MatrixXd L = P_aug.llt().matrixL();
  // Generate sigma points
  Xsig_aug.col(0) = x_aug;  
  for(int i = 0; i < n_aug_; i++) {	  
    Xsig_aug.col(i+1)        = x_aug + sqrt(lambda_+n_aug_) * L.col(i);;
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);;
  }
  
  // Predict sigma points
  for (int i = 0; i< n_sig_; i++)
  {

    double px      = Xsig_aug(0,i);
    double py      = Xsig_aug(1,i);
    double v        = Xsig_aug(2,i);
    double yaw      = Xsig_aug(3,i);
    double yawd     = Xsig_aug(4,i);
    double nu_a     = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    double sin_yaw  = sin(yaw);
    double cos_yaw  = cos(yaw);
    double arg      = yaw + yawd*delta_t;
    
    // Predicted state values
    double px_p, py_p;
    // Avoid division by zero
    if (fabs(yawd) > EPS) {	
      	double v_yawd     = v/yawd;
        px_p              = px + v_yawd * (sin(arg) - sin_yaw);
        py_p              = py + v_yawd * (cos_yaw - cos(arg) );
    }
    else {
	      double v_delta_t = v*delta_t;
        px_p             = px + v_delta_t*cos_yaw;
        py_p             = py + v_delta_t*sin_yaw;
    }
    double v_p    = v;
    double yaw_p  = arg;
    double yawd_p = yawd;

    // Add noise
    px_p    += 0.5*nu_a*delta_t2*cos_yaw;
    py_p    += 0.5*nu_a*delta_t2*sin_yaw;
    v_p     += nu_a*delta_t;
    yaw_p   += 0.5*nu_yawdd*delta_t2;
    yawd_p  += nu_yawdd*delta_t;

    // Output predicted sigma points
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }
  
  // Predicted state mean
  x_ = Xsig_pred_ * weights_; 
  // Predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    NormalizeAngle(&(x_diff(3)));
    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
  }
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {

  int n_z = 3;
  MatrixXd Zsig = MatrixXd(n_z, n_sig_);

  // Sigma points in measurement space
  for (int i = 0; i < n_sig_; i++) {

    double px = Xsig_pred_(0,i);
    double py = Xsig_pred_(1,i);
    double v   = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double v1  = cos(yaw)*v;
    double v2  = sin(yaw)*v;


    Zsig(0,i) = sqrt(pow(px,2) + pow(py,2));          
    Zsig(1,i) = atan2(py,px);                   
    Zsig(2,i) = (px*v1 + py*v2 ) / Zsig(0,i);   
  }

  // Shared steps between Radar and Laser
  UpdateUKF(meas_package, Zsig, n_z);
}


/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {

  int n_z = 2;
  MatrixXd Zsig = Xsig_pred_.block(0, 0, n_z, n_sig_);
  
  // Shared steps between Radar and Laser 
  UpdateUKF(meas_package, Zsig, n_z);
}

// UKF's shared steps 
void UKF::UpdateUKF(MeasurementPackage meas_package, MatrixXd Zsig, int n_z){
  // Mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred  = Zsig * weights_;
  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);
  for (int i = 0; i < n_sig_; i++) { 
    VectorXd z_diff = Zsig.col(i) - z_pred;
    NormalizeAngle(&(z_diff(1)));
    S = S + weights_(i) * z_diff * z_diff.transpose();
  }
  // Add noise covariance matrix
  MatrixXd R = MatrixXd(n_z, n_z);
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR){ 
    R = R_radar_;
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::LASER){ 
    R = R_lidar_;
  }
  S = S + R;
  
  // Cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);
  for (int i = 0; i < n_sig_; i++) { 
    VectorXd z_diff = Zsig.col(i) - z_pred;
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR){ 
      NormalizeAngle(&(z_diff(1)));
    }

    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    NormalizeAngle(&(x_diff(3)));
    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  
  VectorXd z = meas_package.raw_measurements_;  
  MatrixXd K = Tc * S.inverse();  
  VectorXd z_diff = z - z_pred;
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR){
    NormalizeAngle(&(z_diff(1)));
  }

  // Update state and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K * S * K.transpose();
  
}


void UKF::NormalizeAngle(double *ang) {
  while (*ang > M_PI) *ang -= 2. * M_PI;
  while (*ang < -M_PI) *ang += 2. * M_PI;
}