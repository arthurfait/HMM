/*
 * hmm.cpp
 *
 *  Created on: Oct 18, 2012
 *      Author: letrungkien7
 nnn */

#include "hmm.hpp"
#include <iostream>
#include <cmath>
#include <cstdio>
#include <boost/multi_array.hpp>
namespace ltk {

using namespace boost;


HMM::HMM(int n_, int m_) :
														n(n_), m(m_) {
	a.resize(n, n);
	b.resize(n, m);
	pi.resize(n, 1);
}

HMM::~HMM() {

}

double HMM::Evaluate(const MatrixXi &observation, bool logarithm) {
	MatrixXd scale;
	Forward(observation, scale);
	double prob = 0;
	for (int t = 0, T = observation.rows(); t < T; t++)
		prob += log((double) scale(t));
	return logarithm ? exp(prob) : prob;
}

MatrixXd HMM::Forward(const MatrixXi &observation, MatrixXd &scale) {
	int T = observation.rows();
	MatrixXd fwd(T, n);
	scale.resize(T, 1);
	scale.fill(0.0);

	// 1. Initialization
	for (int i = 0; i < n; ++i)
		scale(0) += fwd(0, i) = pi(i) * b(i, observation(0));
	if (scale(0))
		fwd.row(0) /= scale(0);

	// 2. Induction
	for (int t = 1; t < T; ++t) {
		for (int j = 0; j < n; ++j) {
			double sum = 0;
			for (int i = 0; i < n; ++i) {
				sum += fwd(t - 1, i) * a(i, j);
			}
			scale(t) += fwd(t, j) = sum * b(j, observation(t));
		}
		if (scale(t))
			fwd.row(t) /= scale(t);
	}

	return fwd;
}

MatrixXd HMM::Backward(const MatrixXi &observation, const MatrixXd &scale) {
	int T = observation.rows();
	MatrixXd bwd(T, n);

	// 1. Initialization
	bwd.row(T - 1).fill(1.0 / (double) scale(T - 1));

	// 2. Induction
	for (int t = T - 2; t >= 0; t--) {
		for (int i = 0; i < n; ++i) {
			double sum = 0;
			for (int j = 0; j < n; ++j)
				sum += a(i, j) * b(j, observation(t + 1)) * bwd(t + 1, j);
			bwd(t, i) = sum / scale(t);
		}
	}

	return bwd;
}

MatrixXi HMM::Decode(const MatrixXi &observation, double& probability) {
	MatrixXi path;

	return path;
}

typedef multi_array<double, 3> array_type;

double HMM::Learn(vector<MatrixXi> &observation, int maxIteration,
		double tolerance) {
	double newLikelihood, oldLikelihood;
	int K = observation.size();
	vector<MatrixXd> gamma(K);
	vector<array_type> epsilon(K);

	//  Initialization

	for (int k = 0; k < K; k++) {
		int T = observation[k].rows();
		gamma[k].resize(T, n);
		boost::array<array_type::index, 3> shape = {{T,n,n}};
		epsilon[k].resize(shape);
	}
	oldLikelihood = -numeric_limits<double>::max();
	newLikelihood = 0.0;
	int currentIteration = 0;

	bool stop = false;
	do {
		// Calculate fwd, bwd, gamma, epsilon
		for (int k = 0; k < K; k++) {
			MatrixXi &cobservation = observation[k];
			MatrixXd scale;
			MatrixXd &cgamma = gamma[k]; // current gamma

			int T = cobservation.rows();

			// Run fowward, backward
			MatrixXd fwd = Forward(cobservation, scale);
			MatrixXd bwd = Backward(cobservation, scale);

			// Calculate gamma
			for (int t = 0; t < T; t++) {
				double s = 0;
				for (int i = 0; i < n; i++) {
					s += cgamma(t, i) = fwd(t, i) * bwd(t, i);
				}
				if (s)
					cgamma.row(t) /= s;
			}

			// Calculate epsilon
			for (int t = 0; t < T - 1; t++) {
				double s = 0;
				for (int i = 0; i < n; i++) {
					for (int j = 0; j < n; j++) {
						double logvalue = log((double)fwd(t, i)) +log((double) a(i, j))+log((double) b(j, cobservation(t + 1))) + log((double)bwd(t + 1, j));
						s+=epsilon[k][t][i][j] = exp(logvalue);
						//cout <<epsilon[k][t][i][j]<<"  ";
					}
				}
				if (s) {
					for (int i = 0; i < n; i++) {
						for (int j = 0; j < n; j++) {
							epsilon[k][t][i][j] = epsilon[k][t][i][j]/s;
						}
					}
				}
			}


			for (int t = 0; t < T; ++t)
				newLikelihood += log((double)scale(t));
		}

		//cout << "GAMMA"<< endl << gamma[3] <<endl <<endl;
		newLikelihood /= K;
		cout << currentIteration << ": " << newLikelihood << endl;
		if (CheckConvergen(oldLikelihood, newLikelihood, currentIteration,
				maxIteration, tolerance)) {
			stop = true;
		} else {
			currentIteration++;
			oldLikelihood = newLikelihood;
			newLikelihood = 0.0;

			// Update initial probabilities
			for (int i = 0; i < n; ++i) {
				double sum = 0;
				for (int k = 0; k < K; k++)
					sum += gamma[k](0, i);
				pi(i) = sum / K;
			}

			// Update transition
			for (int i = 0; i < n; i++) {

				for (int j = 0; j < n; j++) {
					double denominator = 0, numerator = 0;

					for (int k = 0; k < K; k++) {
						int T = observation[k].rows();

						for (int t = 0; t < T - 1; t++) {
							denominator += epsilon[k][t][i][j];
							numerator += gamma[k](t, i);
						}
					}
					a(i,j) = (numerator) ? denominator/numerator : 0.0;
				}
			}

			// Update emmission probabilities
			for (int j = 0; j < n; j++) {
				for (int l = 0; l < m; ++l) {
					double denominator = 0, numerator = 0;
					for (int k = 0; k < K; k++) {
						int T = observation[k].rows();
						for (int t = 0; t < T; ++t) {
							if (observation[k](t) == l)
								denominator += gamma[k](t, j);
							numerator += gamma[k](t, j);
						}
					}
					b(j, l) = (numerator) ? denominator / numerator : 0.0;
				}
			}
		}
	} while (!stop);
	return newLikelihood;
}

bool HMM::CheckConvergen(double oldLikelihood, double newLikelihood,
		int currentIteration, int maxIteration, double tolerance) {
	// Update and verify stop criteria
	if (tolerance > 0)
	{
		// Stopping criteria is likelihood convergence
		if (abs(oldLikelihood - newLikelihood) <= tolerance)
			return true;

		if (maxIteration > 0)
		{
			// Maximum iterations should also be respected
			if (currentIteration >= maxIteration)
				return true;
		}
	}
	else
	{
		// Stopping criteria is number of iterations
		if (currentIteration == maxIteration)
			return true;
	}

	// Check if we have reached an invalid state
	if (isnan(newLikelihood) || isinf(newLikelihood))
	{
		return true;
	}

	return false;
}


/* Private members access
 *
 */
int HMM::N() const {
	return n;
}
int &HMM::N() {
	return n;
}
int HMM::M() const {
	return m;
}
int &HMM::M() {
	return m;
}
MatrixXd HMM::A() const {
	return a;
}
MatrixXd &HMM::A() {
	return a;
}
MatrixXd HMM::B() const {
	return b;
}
MatrixXd &HMM::B() {
	return b;
}
MatrixXd HMM::PI() const {
	return pi;
}
MatrixXd &HMM::PI() {
	return pi;
}
}
