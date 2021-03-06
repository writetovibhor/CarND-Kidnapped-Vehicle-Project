/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
  num_particles = 100;
  
  default_random_engine gen;
  normal_distribution<double> dist_x(x, std[0]);
  normal_distribution<double> dist_y(y, std[1]);
  normal_distribution<double> dist_theta(theta, std[2]);

  weights.resize(num_particles, 1.0);

	// Add random Gaussian noise to each particle.
  for (int i = 0; i < num_particles; i++) {
    Particle p;
    p.id = i;
    p.x = dist_x(gen);
    p.y = dist_y(gen);
    p.weight = 1.0;
    p.theta = dist_theta(gen);
    particles.push_back(p);
  }
  is_initialized = true;
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/
  default_random_engine gen;
  normal_distribution<double> dist_x(0, std_pos[0]);
  normal_distribution<double> dist_y(0, std_pos[1]);
  normal_distribution<double> dist_theta(0, std_pos[2]);


  // Lesson 12.3
  if (fabs(yaw_rate) < 1e-5) {
    double vdt = velocity * delta_t;

    for (int i = 0; i < particles.size(); i++) {
      particles[i].x += vdt * cos(particles[i].theta);
      particles[i].y += vdt * sin(particles[i].theta);
    }
  } else {
    double v_over_yaw_rate = velocity / yaw_rate;
    double yaw_rate_dt = yaw_rate * delta_t;

    for (int i = 0; i < particles.size(); i++) {
      particles[i].x += v_over_yaw_rate * (sin(particles[i].theta + yaw_rate_dt) - sin(particles[i].theta)) + dist_x(gen);
      particles[i].y += v_over_yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + yaw_rate_dt)) + dist_y(gen);
      particles[i].theta += yaw_rate_dt + dist_theta(gen);
    }
  }

}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.
  for (int io = 0; io < observations.size(); io++) {
    double min_dist = numeric_limits<double>::max();
    int id = -1;
    for (int ip = 0; ip < predicted.size(); ip++) {
      double d = dist(observations[io].x, observations[io].y, predicted[ip].x, predicted[ip].y);
      if (d < min_dist) {
        min_dist = d;
        id = predicted[ip].id;
      }
    }
    observations[io].id = id;
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		std::vector<LandmarkObs> observations, Map map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

  double one_over_two_pi_sigma = (1 / (2 * M_PI * std_landmark[0] * std_landmark[1]));

  for (int ip = 0; ip < particles.size(); ip++) {
    vector<LandmarkObs> predicted_landmarks;

    for (int il = 0; il < map_landmarks.landmark_list.size(); il++) {
      double d = dist(particles[ip].x, particles[ip].y, map_landmarks.landmark_list[il].x_f, map_landmarks.landmark_list[il].y_f);
      if (d < sensor_range) {
        LandmarkObs lm;
        lm.id = map_landmarks.landmark_list[il].id_i;
        lm.x = map_landmarks.landmark_list[il].x_f;
        lm.y = map_landmarks.landmark_list[il].y_f;
        predicted_landmarks.push_back(lm);
      }
    }

    vector<LandmarkObs> transformed_observations;

    for (int io = 0; io < observations.size(); io++) {
      LandmarkObs lm;
      lm.id = observations[io].id;
      lm.x = observations[io].x * cos(particles[ip].theta) - observations[io].y * sin(particles[ip].theta) + particles[ip].x;
      lm.y = observations[io].x * sin(particles[ip].theta) + observations[io].y * cos(particles[ip].theta) + particles[ip].y;
      transformed_observations.push_back(lm);
    }

    dataAssociation(predicted_landmarks, transformed_observations);

    particles[ip].weight = 1.0;

    for (int io = 0; io < transformed_observations.size(); io++) {
      int lm_index = -1;
      for (int il = 0; il < predicted_landmarks.size(); il++) {
        if (predicted_landmarks[il].id == transformed_observations[io].id) {
          lm_index = il;
          break;
        }
      }
      if (lm_index > -1) {
        particles[ip].weight *= one_over_two_pi_sigma * exp(-((pow(transformed_observations[io].x - predicted_landmarks[lm_index].x, 2) / (2 * pow(std_landmark[0], 2))) + (pow(transformed_observations[io].y - predicted_landmarks[lm_index].y, 2) / (2 * pow(std_landmark[1], 2)))));
      }
    }
  }
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
  double sum = 0;
  for (int i = 0; i < num_particles; i++) {
    sum += particles[i].weight;
  }

  default_random_engine gen;
  uniform_int_distribution<int> uniintdist(0, num_particles-1);

  vector<Particle> new_particles;
  while(new_particles.size() < num_particles) {
    int random = uniintdist(gen);
    int count = (int)(particles[random].weight * num_particles / sum);
    new_particles.insert(new_particles.end(), count, particles[random]);
  }
  new_particles.resize(num_particles);

  particles = new_particles;
}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	//Clear the previous associations
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations= associations;
 	particle.sense_x = sense_x;
 	particle.sense_y = sense_y;

 	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
