#pragma once
#include <gtest/gtest.h>
#include "iq_data.h"
#include "RadarProcessor.hpp"
#include "RadarFrame.hpp"
#include <fstream>
#include <sstream>

using namespace std;

vector<float> loadCSV(const string& filename);


void saveRDMToCSV(
    const vector<complex<float>>& rdm,
    const string& filename_real,
    const string& filename_imag,
    size_t rx);